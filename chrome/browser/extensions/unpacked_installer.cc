// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/unpacked_installer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/manifest.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/id_util.h"
#include "sync/api/string_ordinal.h"

#if defined(ENABLE_MANAGED_USERS)
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#endif

using content::BrowserThread;
using extensions::Extension;

namespace {

const char kUnpackedExtensionsBlacklistedError[] =
    "Loading of unpacked extensions is disabled by the administrator.";

// Manages an ExtensionInstallPrompt for a particular extension.
class SimpleExtensionLoadPrompt : public ExtensionInstallPrompt::Delegate {
 public:
  SimpleExtensionLoadPrompt(const Extension* extension,
                            Profile* profile,
                            const base::Closure& callback);
  virtual ~SimpleExtensionLoadPrompt();

  void ShowPrompt();

  // ExtensionInstallUI::Delegate
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

 private:
  scoped_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_refptr<const Extension> extension_;
  base::Closure callback_;
};

SimpleExtensionLoadPrompt::SimpleExtensionLoadPrompt(
    const Extension* extension,
    Profile* profile,
    const base::Closure& callback)
    : install_ui_(ExtensionInstallUI::CreateInstallPromptWithProfile(
          profile)),
      extension_(extension),
      callback_(callback) {
}

SimpleExtensionLoadPrompt::~SimpleExtensionLoadPrompt() {
}

void SimpleExtensionLoadPrompt::ShowPrompt() {
  install_ui_->ConfirmInstall(
      this,
      extension_,
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
}

void SimpleExtensionLoadPrompt::InstallUIProceed() {
  callback_.Run();
  delete this;
}

void SimpleExtensionLoadPrompt::InstallUIAbort(bool user_initiated) {
  delete this;
}

}  // namespace

namespace extensions {

// static
scoped_refptr<UnpackedInstaller> UnpackedInstaller::Create(
    ExtensionService* extension_service) {
  return scoped_refptr<UnpackedInstaller>(
      new UnpackedInstaller(extension_service));
}

UnpackedInstaller::UnpackedInstaller(ExtensionService* extension_service)
    : service_weak_(extension_service->AsWeakPtr()),
      prompt_for_plugins_(true),
      require_modern_manifest_version_(true),
      launch_on_load_(false),
      installer_(extension_service->profile()) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

UnpackedInstaller::~UnpackedInstaller() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
        BrowserThread::CurrentlyOn(BrowserThread::FILE));
}

void UnpackedInstaller::Load(const base::FilePath& path_in) {
  DCHECK(extension_path_.empty());
  extension_path_ = path_in;
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UnpackedInstaller::GetAbsolutePath, this));
}

void UnpackedInstaller::LoadFromCommandLine(const base::FilePath& path_in,
                                            bool launch_on_load) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(extension_path_.empty());

  if (!service_weak_)
    return;
  // Load extensions from the command line synchronously to avoid a race
  // between extension loading and loading an URL from the command line.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  extension_path_ = base::MakeAbsoluteFilePath(path_in);

  if (!IsLoadingUnpackedAllowed()) {
    ReportExtensionLoadError(kUnpackedExtensionsBlacklistedError);
    return;
  }

  std::string error;
  installer_.set_extension(extension_file_util::LoadExtension(
      extension_path_,
      Manifest::COMMAND_LINE,
      GetFlags(),
      &error));

  if (!installer_.extension()) {
    ReportExtensionLoadError(error);
    return;
  }

  launch_on_load_ = launch_on_load;

  ShowInstallPrompt();
}

void UnpackedInstaller::ShowInstallPrompt() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!service_weak_)
    return;

  const ExtensionSet* disabled_extensions =
      service_weak_->disabled_extensions();
  if (service_weak_->show_extensions_prompts() &&
      prompt_for_plugins_ &&
      PluginInfo::HasPlugins(installer_.extension()) &&
      !disabled_extensions->Contains(installer_.extension()->id())) {
    SimpleExtensionLoadPrompt* prompt = new SimpleExtensionLoadPrompt(
        installer_.extension(),
        installer_.profile(),
        base::Bind(&UnpackedInstaller::CallCheckRequirements, this));
    prompt->ShowPrompt();
    return;
  }
  CallCheckRequirements();
}

void UnpackedInstaller::CallCheckRequirements() {
  installer_.CheckRequirements(
      base::Bind(&UnpackedInstaller::OnRequirementsChecked, this));
}

void UnpackedInstaller::OnRequirementsChecked(
    std::vector<std::string> requirement_errors) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!requirement_errors.empty()) {
    ReportExtensionLoadError(JoinString(requirement_errors, ' '));
    return;
  }

#if defined(ENABLE_MANAGED_USERS)
  ManagedUserService* service =
      ManagedUserServiceFactory::GetForProfile(installer_.profile());
  if (service->ProfileIsManaged()) {
    Browser* browser = chrome::FindLastActiveWithProfile(
        service_weak_->profile(),
        chrome::GetActiveDesktop());
    DCHECK(service->CanSkipPassphraseDialog(
        browser->tab_strip_model()->GetActiveWebContents()));
    installer_.OnAuthorizationResult(
        base::Bind(&UnpackedInstaller::ConfirmInstall, this),
        true);
    return;
  }
#endif
  ConfirmInstall();
}

int UnpackedInstaller::GetFlags() {
  std::string id = id_util::GenerateIdForPath(extension_path_);
  bool allow_file_access =
      Manifest::ShouldAlwaysAllowFileAccess(Manifest::UNPACKED);
  ExtensionPrefs* prefs = service_weak_->extension_prefs();
  if (prefs->HasAllowFileAccessSetting(id))
    allow_file_access = prefs->AllowFileAccess(id);

  int result = Extension::FOLLOW_SYMLINKS_ANYWHERE;
  if (allow_file_access)
    result |= Extension::ALLOW_FILE_ACCESS;
  if (require_modern_manifest_version_)
    result |= Extension::REQUIRE_MODERN_MANIFEST_VERSION;

  return result;
}

bool UnpackedInstaller::IsLoadingUnpackedAllowed() const {
  if (!service_weak_)
    return true;
  // If there is a "*" in the extension blacklist, then no extensions should be
  // allowed at all (except explicitly whitelisted extensions).
  ExtensionPrefs* prefs = service_weak_->extension_prefs();
  return !prefs->ExtensionsBlacklistedByDefault();
}

void UnpackedInstaller::GetAbsolutePath() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  extension_path_ = base::MakeAbsoluteFilePath(extension_path_);

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&UnpackedInstaller::CheckExtensionFileAccess, this));
}

void UnpackedInstaller::CheckExtensionFileAccess() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!service_weak_)
    return;

  if (!IsLoadingUnpackedAllowed()) {
    ReportExtensionLoadError(kUnpackedExtensionsBlacklistedError);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&UnpackedInstaller::LoadWithFileAccess, this, GetFlags()));
}

void UnpackedInstaller::LoadWithFileAccess(int flags) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::string error;
  installer_.set_extension(extension_file_util::LoadExtension(
      extension_path_,
      Manifest::UNPACKED,
      flags,
      &error));

  if (!installer_.extension()) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&UnpackedInstaller::ReportExtensionLoadError, this, error));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&UnpackedInstaller::ShowInstallPrompt, this));
}

void UnpackedInstaller::ReportExtensionLoadError(const std::string &error) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!service_weak_)
    return;
  service_weak_->ReportExtensionLoadError(extension_path_, error, true);
}

void UnpackedInstaller::ConfirmInstall() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  string16 error = installer_.CheckManagementPolicy();
  if (!error.empty()) {
    ReportExtensionLoadError(UTF16ToASCII(error));
    return;
  }

  PermissionsUpdater perms_updater(service_weak_->profile());
  perms_updater.GrantActivePermissions(installer_.extension());

  if (launch_on_load_)
    service_weak_->ScheduleLaunchOnLoad(installer_.extension()->id());

  service_weak_->OnExtensionInstalled(
      installer_.extension(),
      syncer::StringOrdinal(),
      false /* no requirement errors */,
      false /* don't wait for idle */);
}

}  // namespace extensions
