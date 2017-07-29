// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/unpacked_installer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_install_ui_factory.h"
#include "components/crx_file/id_util.h"
#include "components/sync/model/string_ordinal.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/extension_install_ui.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/policy_check.h"
#include "extensions/browser/preload_check_group.h"
#include "extensions/browser/requirements_checker.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/plugins_handler.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/permissions/permissions_data.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::SharedModuleInfo;

namespace {

const char kUnpackedExtensionsBlacklistedError[] =
    "Loading of unpacked extensions is disabled by the administrator.";

const char kImportMinVersionNewer[] =
    "'import' version requested is newer than what is installed.";
const char kImportMissing[] = "'import' extension is not installed.";
const char kImportNotSharedModule[] = "'import' is not a shared module.";

// Manages an ExtensionInstallPrompt for a particular extension.
class SimpleExtensionLoadPrompt {
 public:
  SimpleExtensionLoadPrompt(const Extension* extension,
                            Profile* profile,
                            const base::Closure& callback);

  void ShowPrompt();

 private:
  ~SimpleExtensionLoadPrompt();  // Manages its own lifetime.

  void OnInstallPromptDone(ExtensionInstallPrompt::Result result);

  std::unique_ptr<ExtensionInstallPrompt> install_ui_;
  scoped_refptr<const Extension> extension_;
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(SimpleExtensionLoadPrompt);
};

SimpleExtensionLoadPrompt::SimpleExtensionLoadPrompt(
    const Extension* extension,
    Profile* profile,
    const base::Closure& callback)
    : extension_(extension), callback_(callback) {
  std::unique_ptr<extensions::ExtensionInstallUI> ui(
      extensions::CreateExtensionInstallUI(profile));
  install_ui_.reset(new ExtensionInstallPrompt(
      profile, ui->GetDefaultInstallDialogParent()));
}

SimpleExtensionLoadPrompt::~SimpleExtensionLoadPrompt() {
}

void SimpleExtensionLoadPrompt::ShowPrompt() {
  // Unretained() is safe because this object manages its own lifetime.
  install_ui_->ShowDialog(
      base::Bind(&SimpleExtensionLoadPrompt::OnInstallPromptDone,
                 base::Unretained(this)),
      extension_.get(), nullptr,
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
}

void SimpleExtensionLoadPrompt::OnInstallPromptDone(
    ExtensionInstallPrompt::Result result) {
  if (result == ExtensionInstallPrompt::Result::ACCEPTED)
    callback_.Run();
  delete this;
}

}  // namespace

namespace extensions {

// static
scoped_refptr<UnpackedInstaller> UnpackedInstaller::Create(
    ExtensionService* extension_service) {
  DCHECK(extension_service);
  return scoped_refptr<UnpackedInstaller>(
      new UnpackedInstaller(extension_service));
}

UnpackedInstaller::UnpackedInstaller(ExtensionService* extension_service)
    : service_weak_(extension_service->AsWeakPtr()),
      profile_(extension_service->profile()),
      prompt_for_plugins_(true),
      require_modern_manifest_version_(true),
      be_noisy_on_failure_(true) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

UnpackedInstaller::~UnpackedInstaller() {
}

void UnpackedInstaller::Load(const base::FilePath& path_in) {
  DCHECK(extension_path_.empty());
  extension_path_ = path_in;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::BindOnce(&UnpackedInstaller::GetAbsolutePath, this));
}

bool UnpackedInstaller::LoadFromCommandLine(const base::FilePath& path_in,
                                            std::string* extension_id,
                                            bool only_allow_apps) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(extension_path_.empty());

  if (!service_weak_.get())
    return false;
  // Load extensions from the command line synchronously to avoid a race
  // between extension loading and loading an URL from the command line.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  extension_path_ = base::MakeAbsoluteFilePath(path_in);

  if (!IsLoadingUnpackedAllowed()) {
    ReportExtensionLoadError(kUnpackedExtensionsBlacklistedError);
    return false;
  }

  std::string error;
  extension_ = file_util::LoadExtension(extension_path_, Manifest::COMMAND_LINE,
                                        GetFlags(), &error);

  if (!extension() ||
      !extension_l10n_util::ValidateExtensionLocales(
          extension_path_, extension()->manifest()->value(), &error)) {
    ReportExtensionLoadError(error);
    return false;
  }

  if (only_allow_apps && !extension()->is_platform_app()) {
#if defined(GOOGLE_CHROME_BUILD)
    // Avoid crashing for users with hijacked shortcuts.
    return true;
#else
    // Defined here to avoid unused variable errors in official builds.
    const char extension_instead_of_app_error[] =
        "App loading flags cannot be used to load extensions. Please use "
        "--load-extension instead.";
    ReportExtensionLoadError(extension_instead_of_app_error);
    return false;
#endif
  }

  extension()->permissions_data()->BindToCurrentThread();
  PermissionsUpdater(
      service_weak_->profile(), PermissionsUpdater::INIT_FLAG_TRANSIENT)
      .InitializePermissions(extension());
  ShowInstallPrompt();

  *extension_id = extension()->id();
  return true;
}

void UnpackedInstaller::ShowInstallPrompt() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_weak_.get())
    return;

  const ExtensionSet& disabled_extensions =
      ExtensionRegistry::Get(service_weak_->profile())->disabled_extensions();
  if (prompt_for_plugins_ &&
      PluginInfo::HasPlugins(extension()) &&
      !disabled_extensions.Contains(extension()->id())) {
    SimpleExtensionLoadPrompt* prompt = new SimpleExtensionLoadPrompt(
        extension(), profile_,
        base::Bind(&UnpackedInstaller::StartInstallChecks, this));
    prompt->ShowPrompt();
    return;
  }
  StartInstallChecks();
}

void UnpackedInstaller::StartInstallChecks() {
  // TODO(crbug.com/421128): Enable these checks all the time.  The reason
  // they are disabled for extensions loaded from the command-line is that
  // installing unpacked extensions is asynchronous, but there can be
  // dependencies between the extensions loaded by the command line.
  if (extension()->manifest()->location() != Manifest::COMMAND_LINE) {
    ExtensionService* service = service_weak_.get();
    if (!service || service->browser_terminating())
      return;

    // TODO(crbug.com/420147): Move this code to a utility class to avoid
    // duplication of SharedModuleService::CheckImports code.
    if (SharedModuleInfo::ImportsModules(extension())) {
      const std::vector<SharedModuleInfo::ImportInfo>& imports =
          SharedModuleInfo::GetImports(extension());
      std::vector<SharedModuleInfo::ImportInfo>::const_iterator i;
      for (i = imports.begin(); i != imports.end(); ++i) {
        base::Version version_required(i->minimum_version);
        const Extension* imported_module =
            service->GetExtensionById(i->extension_id, true);
        if (!imported_module) {
          ReportExtensionLoadError(kImportMissing);
          return;
        } else if (imported_module &&
                   !SharedModuleInfo::IsSharedModule(imported_module)) {
          ReportExtensionLoadError(kImportNotSharedModule);
          return;
        } else if (imported_module && (version_required.IsValid() &&
                                       imported_module->version()->CompareTo(
                                           version_required) < 0)) {
          ReportExtensionLoadError(kImportMinVersionNewer);
          return;
        }
      }
    }
  }

  policy_check_ = base::MakeUnique<PolicyCheck>(profile_, extension_);
  requirements_check_ = base::MakeUnique<RequirementsChecker>(extension_);

  check_group_ = base::MakeUnique<PreloadCheckGroup>();
  check_group_->set_stop_on_first_error(true);

  check_group_->AddCheck(policy_check_.get());
  check_group_->AddCheck(requirements_check_.get());
  check_group_->Start(
      base::BindOnce(&UnpackedInstaller::OnInstallChecksComplete, this));
}

void UnpackedInstaller::OnInstallChecksComplete(PreloadCheck::Errors errors) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (errors.empty()) {
    InstallExtension();
    return;
  }

  base::string16 error_message;
  if (errors.count(PreloadCheck::DISALLOWED_BY_POLICY))
    error_message = policy_check_->GetErrorMessage();
  else
    error_message = requirements_check_->GetErrorMessage();

  DCHECK(!error_message.empty());
  ReportExtensionLoadError(base::UTF16ToUTF8(error_message));
}

int UnpackedInstaller::GetFlags() {
  std::string id = crx_file::id_util::GenerateIdForPath(extension_path_);
  bool allow_file_access =
      Manifest::ShouldAlwaysAllowFileAccess(Manifest::UNPACKED);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(service_weak_->profile());
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
  if (!service_weak_.get())
    return true;
  // If there is a "*" in the extension blacklist, then no extensions should be
  // allowed at all (except explicitly whitelisted extensions).
  return !ExtensionManagementFactory::GetForBrowserContext(
              service_weak_->profile())->BlacklistedByDefault();
}

void UnpackedInstaller::GetAbsolutePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  extension_path_ = base::MakeAbsoluteFilePath(extension_path_);

  std::string error;
  if (!file_util::CheckForIllegalFilenames(extension_path_, &error)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&UnpackedInstaller::ReportExtensionLoadError, this,
                       error));
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&UnpackedInstaller::CheckExtensionFileAccess, this));
}

void UnpackedInstaller::CheckExtensionFileAccess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_weak_.get())
    return;

  if (!IsLoadingUnpackedAllowed()) {
    ReportExtensionLoadError(kUnpackedExtensionsBlacklistedError);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::BindOnce(&UnpackedInstaller::LoadWithFileAccess, this, GetFlags()));
}

void UnpackedInstaller::LoadWithFileAccess(int flags) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  std::string error;
  extension_ = file_util::LoadExtension(extension_path_, Manifest::UNPACKED,
                                        flags, &error);

  if (!extension() ||
      !extension_l10n_util::ValidateExtensionLocales(
          extension_path_, extension()->manifest()->value(), &error)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&UnpackedInstaller::ReportExtensionLoadError, this,
                       error));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&UnpackedInstaller::ShowInstallPrompt, this));
}

void UnpackedInstaller::ReportExtensionLoadError(const std::string &error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (service_weak_.get()) {
    ExtensionErrorReporter::GetInstance()->ReportLoadError(
        extension_path_,
        error,
        service_weak_->profile(),
        be_noisy_on_failure_);
  }

  if (!callback_.is_null()) {
    callback_.Run(nullptr, extension_path_, error);
    callback_.Reset();
  }
}

void UnpackedInstaller::InstallExtension() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!service_weak_.get()) {
    callback_.Reset();
    return;
  }

  PermissionsUpdater perms_updater(service_weak_->profile());
  perms_updater.InitializePermissions(extension());
  perms_updater.GrantActivePermissions(extension());

  service_weak_->OnExtensionInstalled(
      extension(), syncer::StringOrdinal(), kInstallFlagInstallImmediately);

  if (!callback_.is_null()) {
    callback_.Run(extension(), extension_path_, std::string());
    callback_.Reset();
  }
}

}  // namespace extensions
