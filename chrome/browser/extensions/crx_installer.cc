// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"

#include <map>
#include <set>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/stringprintf.h"
#include "base/task.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/convert_user_script.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

struct WhitelistedInstallData {
  WhitelistedInstallData() {}
  std::set<std::string> ids;
  std::map<std::string, linked_ptr<DictionaryValue> > manifests;
};

static base::LazyInstance<WhitelistedInstallData>
    g_whitelisted_install_data(base::LINKER_INITIALIZED);

}  // namespace

// static
void CrxInstaller::SetWhitelistedInstallId(const std::string& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_whitelisted_install_data.Get().ids.insert(id);
}

// static
void CrxInstaller::SetWhitelistedManifest(const std::string& id,
                                          DictionaryValue* parsed_manifest) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WhitelistedInstallData& data = g_whitelisted_install_data.Get();
  data.manifests[id] = linked_ptr<DictionaryValue>(parsed_manifest);
}

// static
const DictionaryValue* CrxInstaller::GetWhitelistedManifest(
    const std::string& id) {
  WhitelistedInstallData& data = g_whitelisted_install_data.Get();
  if (ContainsKey(data.manifests, id))
    return data.manifests[id].get();
  else
    return NULL;
}

// static
DictionaryValue* CrxInstaller::RemoveWhitelistedManifest(
    const std::string& id) {
  WhitelistedInstallData& data = g_whitelisted_install_data.Get();
  if (ContainsKey(data.manifests, id)) {
    DictionaryValue* manifest = data.manifests[id].release();
    data.manifests.erase(id);
    return manifest;
  }
  return NULL;
}

// static
bool CrxInstaller::IsIdWhitelisted(const std::string& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<std::string>& ids = g_whitelisted_install_data.Get().ids;
  return ContainsKey(ids, id);
}

// static
bool CrxInstaller::ClearWhitelistedInstallId(const std::string& id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<std::string>& ids = g_whitelisted_install_data.Get().ids;
  if (ContainsKey(ids, id)) {
    ids.erase(id);
    return true;
  }
  return false;
}

CrxInstaller::CrxInstaller(ExtensionService* frontend,
                           ExtensionInstallUI* client)
    : install_directory_(frontend->install_directory()),
      install_source_(Extension::INTERNAL),
      extensions_enabled_(frontend->extensions_enabled()),
      delete_source_(false),
      is_gallery_install_(false),
      create_app_shortcut_(false),
      frontend_(frontend),
      client_(client),
      apps_require_extension_mime_type_(false),
      allow_silent_install_(false) {
}

CrxInstaller::~CrxInstaller() {
  // Delete the temp directory and crx file as necessary. Note that the
  // destructor might be called on any thread, so we post a task to the file
  // thread to make sure the delete happens there.
  if (!temp_dir_.value().empty()) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(
            &extension_file_util::DeleteFile, temp_dir_, true));
  }

  if (delete_source_) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableFunction(
            &extension_file_util::DeleteFile, source_file_, false));
  }

  // Make sure the UI is deleted on the ui thread.
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, client_);
  client_ = NULL;
}

void CrxInstaller::InstallCrx(const FilePath& source_file) {
  source_file_ = source_file;

  scoped_refptr<SandboxedExtensionUnpacker> unpacker(
      new SandboxedExtensionUnpacker(
          source_file,
          g_browser_process->resource_dispatcher_host(),
          this));

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          unpacker.get(), &SandboxedExtensionUnpacker::Start));
}

void CrxInstaller::InstallUserScript(const FilePath& source_file,
                                     const GURL& original_url) {
  DCHECK(!original_url.is_empty());

  source_file_ = source_file;
  original_url_ = original_url;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &CrxInstaller::ConvertUserScriptOnFileThread));
}

void CrxInstaller::ConvertUserScriptOnFileThread() {
  std::string error;
  scoped_refptr<Extension> extension =
      ConvertUserScriptToExtension(source_file_, original_url_, &error);
  if (!extension) {
    ReportFailureFromFileThread(error);
    return;
  }

  OnUnpackSuccess(extension->path(), extension->path(), extension);
}

void CrxInstaller::InstallWebApp(const WebApplicationInfo& web_app) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &CrxInstaller::ConvertWebAppOnFileThread,
                        web_app));
}

void CrxInstaller::ConvertWebAppOnFileThread(
    const WebApplicationInfo& web_app) {
  std::string error;
  scoped_refptr<Extension> extension(
      ConvertWebAppToExtension(web_app, base::Time::Now()));
  if (!extension) {
    // Validation should have stopped any potential errors before getting here.
    NOTREACHED() << "Could not convert web app to extension.";
    return;
  }

  // TODO(aa): conversion data gets lost here :(

  OnUnpackSuccess(extension->path(), extension->path(), extension);
}

bool CrxInstaller::AllowInstall(const Extension* extension,
                                std::string* error) {
  DCHECK(error);

  // Make sure the expected id matches.
  if (!expected_id_.empty() && expected_id_ != extension->id()) {
    *error = base::StringPrintf(
        "ID in new CRX manifest (%s) does not match expected id (%s)",
        extension->id().c_str(),
        expected_id_.c_str());
    return false;
  }

  if (expected_version_.get() &&
      !expected_version_->Equals(*extension->version())) {
    *error = base::StringPrintf(
        "Version in new CRX %s manifest (%s) does not match expected "
        "version (%s)",
        extension->id().c_str(),
        expected_version_->GetString().c_str(),
        extension->version()->GetString().c_str());
    return false;
  }

  // The checks below are skipped for themes and external installs.
  if (extension->is_theme() || Extension::IsExternalLocation(install_source_))
    return true;

  if (!extensions_enabled_) {
    *error = "Extensions are not enabled.";
    return false;
  }

  if (extension_->is_app()) {
    // If the app was downloaded, apps_require_extension_mime_type_
    // will be set.  In this case, check that it was served with the
    // right mime type.  Make an exception for file URLs, which come
    // from the users computer and have no headers.
    if (!original_url_.SchemeIsFile() &&
        apps_require_extension_mime_type_ &&
        original_mime_type_ != Extension::kMimeType) {
      *error = base::StringPrintf(
          "Apps must be served with content type %s.",
          Extension::kMimeType);
      return false;
    }

    // If the client_ is NULL, then the app is either being installed via
    // an internal mechanism like sync, external_extensions, or default apps.
    // In that case, we don't want to enforce things like the install origin.
    if (!is_gallery_install_ && client_) {
      // For apps with a gallery update URL, require that they be installed
      // from the gallery.
      // TODO(erikkay) Apply this rule for paid extensions and themes as well.
      if (extension->UpdatesFromGallery()) {
        *error = l10n_util::GetStringFUTF8(
            IDS_EXTENSION_DISALLOW_NON_DOWNLOADED_GALLERY_INSTALLS,
            l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
        return false;
      }

      // For self-hosted apps, verify that the entire extent is on the same
      // host (or a subdomain of the host) the download happened from.  There's
      // no way for us to verify that the app controls any other hosts.
      URLPattern pattern(UserScript::kValidUserScriptSchemes);
      pattern.set_host(original_url_.host());
      pattern.set_match_subdomains(true);

      ExtensionExtent::PatternList patterns =
          extension_->web_extent().patterns();
      for (size_t i = 0; i < patterns.size(); ++i) {
        if (!pattern.MatchesHost(patterns[i].host())) {
          *error = base::StringPrintf(
              "Apps must be served from the host that they affect.");
          return false;
        }
      }
    }
  }

  return true;
}

void CrxInstaller::OnUnpackFailure(const std::string& error_message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  ReportFailureFromFileThread(error_message);
}

void CrxInstaller::OnUnpackSuccess(const FilePath& temp_dir,
                                   const FilePath& extension_dir,
                                   const Extension* extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Note: We take ownership of |extension| and |temp_dir|.
  extension_ = extension;
  temp_dir_ = temp_dir;

  // We don't have to delete the unpack dir explicity since it is a child of
  // the temp dir.
  unpacked_extension_root_ = extension_dir;

  std::string error;
  if (!AllowInstall(extension, &error)) {
    ReportFailureFromFileThread(error);
    return;
  }

  if (client_) {
    Extension::DecodeIcon(extension_.get(), Extension::EXTENSION_ICON_LARGE,
                          &install_icon_);
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &CrxInstaller::ConfirmInstall));
}

// Helper method to let us compare a whitelisted manifest with the actual
// downloaded extension's manifest, but ignoring the kPublicKey since the
// whitelisted manifest doesn't have that value.
static bool EqualsIgnoringPublicKey(
    const DictionaryValue& extension_manifest,
    const DictionaryValue& whitelisted_manifest) {
  scoped_ptr<DictionaryValue> manifest_copy(extension_manifest.DeepCopy());
  manifest_copy->Remove(extension_manifest_keys::kPublicKey, NULL);
  return manifest_copy->Equals(&whitelisted_manifest);
}

void CrxInstaller::ConfirmInstall() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (frontend_->extension_prefs()->IsExtensionBlacklisted(extension_->id())) {
    VLOG(1) << "This extension: " << extension_->id()
            << " is blacklisted. Install failed.";
    ReportFailureFromUIThread("This extension is blacklisted.");
    return;
  }

  if (!frontend_->extension_prefs()->IsExtensionAllowedByPolicy(
      extension_->id())) {
    ReportFailureFromUIThread("This extension is blacklisted by admin policy.");
    return;
  }

  GURL overlapping_url;
  const Extension* overlapping_extension =
      frontend_->GetExtensionByOverlappingWebExtent(extension_->web_extent());
  if (overlapping_extension &&
      overlapping_extension->id() != extension_->id()) {
    ReportFailureFromUIThread(l10n_util::GetStringFUTF8(
        IDS_EXTENSION_OVERLAPPING_WEB_EXTENT,
        UTF8ToUTF16(overlapping_extension->name())));
    return;
  }

  current_version_ =
      frontend_->extension_prefs()->GetVersionString(extension_->id());

  // First see if it's whitelisted by id (the old mechanism).
  bool whitelisted = ClearWhitelistedInstallId(extension_->id()) &&
      extension_->plugins().empty() && is_gallery_install_;

  // Now check if it's whitelisted by manifest.
  scoped_ptr<DictionaryValue> whitelisted_manifest(
      RemoveWhitelistedManifest(extension_->id()));
  if (is_gallery_install_ && whitelisted_manifest.get()) {
    if (!EqualsIgnoringPublicKey(*extension_->manifest_value(),
                                 *whitelisted_manifest)) {
      ReportFailureFromUIThread(
          l10n_util::GetStringUTF8(IDS_EXTENSION_MANIFEST_INVALID));
      return;
    }
    whitelisted = true;
  }

  if (client_ &&
      (!allow_silent_install_ || !whitelisted)) {
    AddRef();  // Balanced in Proceed() and Abort().
    client_->ConfirmInstall(this, extension_.get());
  } else {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &CrxInstaller::CompleteInstall));
  }
  return;
}

void CrxInstaller::InstallUIProceed() {
  BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &CrxInstaller::CompleteInstall));

  Release();  // balanced in ConfirmInstall().
}

void CrxInstaller::InstallUIAbort() {
  // Technically, this can be called for other reasons than the user hitting
  // cancel, but they're rare.
  ExtensionService::RecordPermissionMessagesHistogram(
      extension_, "Extensions.Permissions_InstallCancel");

  // Kill the theme loading bubble.
  NotificationService* service = NotificationService::current();
  service->Notify(NotificationType::NO_THEME_DETECTED,
                  Source<CrxInstaller>(this),
                  NotificationService::NoDetails());
  Release();  // balanced in ConfirmInstall().

  // We're done. Since we don't post any more tasks to ourself, our ref count
  // should go to zero and we die. The destructor will clean up the temp dir.
}

void CrxInstaller::CompleteInstall() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!current_version_.empty()) {
    scoped_ptr<Version> current_version(
        Version::GetVersionFromString(current_version_));
    if (current_version->CompareTo(*(extension_->version())) > 0) {
      ReportFailureFromFileThread("Attempted to downgrade extension.");
      return;
    }
  }

  // See how long extension install paths are.  This is important on
  // windows, because file operations may fail if the path to a file
  // exceeds a small constant.  See crbug.com/69693 .
  UMA_HISTOGRAM_CUSTOM_COUNTS(
    "Extensions.CrxInstallDirPathLength",
        install_directory_.value().length(), 0, 500, 100);

  FilePath version_dir = extension_file_util::InstallExtension(
      unpacked_extension_root_,
      extension_->id(),
      extension_->VersionString(),
      install_directory_);
  if (version_dir.empty()) {
    ReportFailureFromFileThread(
        l10n_util::GetStringUTF8(
            IDS_EXTENSION_MOVE_DIRECTORY_TO_PROFILE_FAILED));
    return;
  }

  // This is lame, but we must reload the extension because absolute paths
  // inside the content scripts are established inside InitFromValue() and we
  // just moved the extension.
  // TODO(aa): All paths to resources inside extensions should be created
  // lazily and based on the Extension's root path at that moment.
  std::string error;
  extension_ = extension_file_util::LoadExtension(
      version_dir,
      install_source_,
      Extension::REQUIRE_KEY,
      &error);
  CHECK(error.empty()) << error;

  ReportSuccessFromFileThread();
}

void CrxInstaller::ReportFailureFromFileThread(const std::string& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &CrxInstaller::ReportFailureFromUIThread, error));
}

void CrxInstaller::ReportFailureFromUIThread(const std::string& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  NotificationService* service = NotificationService::current();
  service->Notify(NotificationType::EXTENSION_INSTALL_ERROR,
                  Source<CrxInstaller>(this),
                  Details<const std::string>(&error));

  // This isn't really necessary, it is only used because unit tests expect to
  // see errors get reported via this interface.
  //
  // TODO(aa): Need to go through unit tests and clean them up too, probably get
  // rid of this line.
  ExtensionErrorReporter::GetInstance()->ReportError(error, false);  // quiet

  if (client_)
    client_->OnInstallFailure(error);
}

void CrxInstaller::ReportSuccessFromFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this, &CrxInstaller::ReportSuccessFromUIThread));
}

void CrxInstaller::ReportSuccessFromUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If there is a client, tell the client about installation.
  if (client_)
    client_->OnInstallSuccess(extension_.get(), install_icon_.get());

  // Tell the frontend about the installation and hand off ownership of
  // extension_ to it.
  frontend_->OnExtensionInstalled(extension_);
  extension_ = NULL;

  // We're done. We don't post any more tasks to ourselves so we are deleted
  // soon.
}
