// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/convert_user_script.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;

namespace {

struct Whitelist {
  Whitelist() {}
  std::set<std::string> ids;
  std::map<std::string, linked_ptr<CrxInstaller::WhitelistEntry> > entries;
};

static base::LazyInstance<Whitelist>
    g_whitelisted_install_data = LAZY_INSTANCE_INITIALIZER;

}  // namespace

CrxInstaller::WhitelistEntry::WhitelistEntry()
    : use_app_installed_bubble(false),
      skip_post_install_ui(false) {}
CrxInstaller::WhitelistEntry::~WhitelistEntry() {}

// static
scoped_refptr<CrxInstaller> CrxInstaller::Create(
    ExtensionService* frontend,
    ExtensionInstallUI* client) {
  return new CrxInstaller(frontend->AsWeakPtr(), client);
}

// static
void CrxInstaller::SetWhitelistedInstallId(const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  g_whitelisted_install_data.Get().ids.insert(id);
}

// static
void CrxInstaller::SetWhitelistEntry(const std::string& id,
                                     CrxInstaller::WhitelistEntry* entry) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Whitelist& data = g_whitelisted_install_data.Get();
  data.entries[id] = linked_ptr<CrxInstaller::WhitelistEntry>(entry);
}

// static
const CrxInstaller::WhitelistEntry* CrxInstaller::GetWhitelistEntry(
    const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Whitelist& data = g_whitelisted_install_data.Get();
  if (ContainsKey(data.entries, id))
    return data.entries[id].get();
  else
    return NULL;
}

// static
CrxInstaller::WhitelistEntry* CrxInstaller::RemoveWhitelistEntry(
    const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Whitelist& data = g_whitelisted_install_data.Get();
  if (ContainsKey(data.entries, id)) {
    CrxInstaller::WhitelistEntry* entry = data.entries[id].release();
    data.entries.erase(id);
    return entry;
  }
  return NULL;
}

// static
bool CrxInstaller::IsIdWhitelisted(const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<std::string>& ids = g_whitelisted_install_data.Get().ids;
  return ContainsKey(ids, id);
}

// static
bool CrxInstaller::ClearWhitelistedInstallId(const std::string& id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<std::string>& ids = g_whitelisted_install_data.Get().ids;
  if (ContainsKey(ids, id)) {
    ids.erase(id);
    return true;
  }
  return false;
}

CrxInstaller::CrxInstaller(base::WeakPtr<ExtensionService> frontend_weak,
                           ExtensionInstallUI* client)
    : install_directory_(frontend_weak->install_directory()),
      install_source_(Extension::INTERNAL),
      extensions_enabled_(frontend_weak->extensions_enabled()),
      delete_source_(false),
      create_app_shortcut_(false),
      page_index_(-1),
      frontend_weak_(frontend_weak),
      profile_(frontend_weak->profile()),
      client_(client),
      apps_require_extension_mime_type_(false),
      allow_silent_install_(false),
      install_cause_(extension_misc::INSTALL_CAUSE_UNSET),
      creation_flags_(Extension::NO_FLAGS) {
}

CrxInstaller::~CrxInstaller() {
  // Delete the temp directory and crx file as necessary. Note that the
  // destructor might be called on any thread, so we post a task to the file
  // thread to make sure the delete happens there. This is a best effort
  // operation since the browser can be shutting down so there might not
  // be a file thread to post to.
  if (!temp_dir_.value().empty()) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&extension_file_util::DeleteFile, temp_dir_, true));
  }
  if (delete_source_) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&extension_file_util::DeleteFile, source_file_, false));
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
          install_source_,
          creation_flags_,
          this));

  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(
              &SandboxedExtensionUnpacker::Start, unpacker.get())))
    NOTREACHED();
}

void CrxInstaller::InstallUserScript(const FilePath& source_file,
                                     const GURL& download_url) {
  DCHECK(!download_url.is_empty());

  source_file_ = source_file;
  download_url_ = download_url;

  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&CrxInstaller::ConvertUserScriptOnFileThread, this)))
    NOTREACHED();
}

void CrxInstaller::ConvertUserScriptOnFileThread() {
  std::string error;
  scoped_refptr<Extension> extension =
      ConvertUserScriptToExtension(source_file_, download_url_, &error);
  if (!extension) {
    ReportFailureFromFileThread(error);
    return;
  }

  OnUnpackSuccess(extension->path(), extension->path(), NULL, extension);
}

void CrxInstaller::InstallWebApp(const WebApplicationInfo& web_app) {
  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&CrxInstaller::ConvertWebAppOnFileThread, this, web_app)))
    NOTREACHED();
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

  OnUnpackSuccess(extension->path(), extension->path(), NULL, extension);
}

bool CrxInstaller::AllowInstall(const Extension* extension,
                                std::string* error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
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
    if (!download_url_.SchemeIsFile() &&
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
    if (!is_gallery_install() && client_) {
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
      pattern.SetHost(download_url_.host());
      pattern.SetMatchSubdomains(true);

      URLPatternSet patterns = extension_->web_extent();
      for (URLPatternSet::const_iterator i = patterns.begin();
           i != patterns.end(); ++i) {
        if (!pattern.MatchesHost(i->host())) {
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

  UMA_HISTOGRAM_ENUMERATION("Extensions.UnpackFailureInstallSource",
                            install_source(), Extension::NUM_LOCATIONS);


  UMA_HISTOGRAM_ENUMERATION("Extensions.UnpackFailureInstallCause",
                            install_cause(),
                            extension_misc::NUM_INSTALL_CAUSES);

  ReportFailureFromFileThread(error_message);
}

void CrxInstaller::OnUnpackSuccess(const FilePath& temp_dir,
                                   const FilePath& extension_dir,
                                   const DictionaryValue* original_manifest,
                                   const Extension* extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  UMA_HISTOGRAM_ENUMERATION("Extensions.UnpackSuccessInstallSource",
                            install_source(), Extension::NUM_LOCATIONS);


  UMA_HISTOGRAM_ENUMERATION("Extensions.UnpackSuccessInstallCause",
                            install_cause(),
                            extension_misc::NUM_INSTALL_CAUSES);

  // Note: We take ownership of |extension| and |temp_dir|.
  extension_ = extension;
  temp_dir_ = temp_dir;

  if (original_manifest)
    original_manifest_.reset(original_manifest->DeepCopy());

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

  if (!BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&CrxInstaller::ConfirmInstall, this)))
    NOTREACHED();
}

void CrxInstaller::ConfirmInstall() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!frontend_weak_.get())
    return;

  if (frontend_weak_->extension_prefs()
      ->IsExtensionBlacklisted(extension_->id())) {
    VLOG(1) << "This extension: " << extension_->id()
            << " is blacklisted. Install failed.";
    ReportFailureFromUIThread(
        l10n_util::GetStringUTF8(IDS_EXTENSION_CANT_INSTALL_BLACKLISTED));
    return;
  }

  if (!frontend_weak_->extension_prefs()->IsExtensionAllowedByPolicy(
          extension_->id(), install_source_)) {
    ReportFailureFromUIThread(
        l10n_util::GetStringUTF8(IDS_EXTENSION_CANT_INSTALL_POLICY_BLACKLIST));
    return;
  }

  GURL overlapping_url;
  const Extension* overlapping_extension =
      frontend_weak_->
      GetExtensionByOverlappingWebExtent(extension_->web_extent());
  if (overlapping_extension &&
      overlapping_extension->id() != extension_->id()) {
    ReportFailureFromUIThread(l10n_util::GetStringFUTF8(
        IDS_EXTENSION_OVERLAPPING_WEB_EXTENT,
        UTF8ToUTF16(overlapping_extension->name())));
    return;
  }

  current_version_ =
      frontend_weak_->extension_prefs()->GetVersionString(extension_->id());

  // TODO(asargent) - remove this when we fully deprecate the old install api.
  ClearWhitelistedInstallId(extension_->id());

  bool whitelisted = false;
  scoped_ptr<CrxInstaller::WhitelistEntry> entry(
      RemoveWhitelistEntry(extension_->id()));
  if (is_gallery_install() && entry.get() && original_manifest_.get()) {
    if (!(original_manifest_->Equals(entry->parsed_manifest.get()))) {
      ReportFailureFromUIThread(
          l10n_util::GetStringUTF8(IDS_EXTENSION_MANIFEST_INVALID));
      return;
    }
    whitelisted = true;
    if (entry->use_app_installed_bubble)
      client_->set_use_app_installed_bubble(true);
    if (entry->skip_post_install_ui)
      client_->set_skip_post_install_ui(true);
  }

  if (client_ &&
      (!allow_silent_install_ || !whitelisted)) {
    AddRef();  // Balanced in Proceed() and Abort().
    client_->ConfirmInstall(this, extension_.get());
  } else {
    if (!BrowserThread::PostTask(
            BrowserThread::FILE, FROM_HERE,
            base::Bind(&CrxInstaller::CompleteInstall, this)))
      NOTREACHED();
  }
  return;
}

void CrxInstaller::InstallUIProceed() {
  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&CrxInstaller::CompleteInstall, this)))
    NOTREACHED();

  Release();  // balanced in ConfirmInstall().
}

void CrxInstaller::InstallUIAbort(bool user_initiated) {
  std::string histogram_name = user_initiated ?
      "Extensions.Permissions_InstallCancel" :
      "Extensions.Permissions_InstallAbort";
  ExtensionService::RecordPermissionMessagesHistogram(
      extension_, histogram_name.c_str());

  // Kill the theme loading bubble.
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_NO_THEME_DETECTED,
                  content::Source<CrxInstaller>(this),
                  content::NotificationService::NoDetails());
  Release();  // balanced in ConfirmInstall().

  NotifyCrxInstallComplete(NULL);

  // We're done. Since we don't post any more tasks to ourself, our ref count
  // should go to zero and we die. The destructor will clean up the temp dir.
}

void CrxInstaller::CompleteInstall() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!current_version_.empty()) {
    scoped_ptr<Version> current_version(
        Version::GetVersionFromString(current_version_));
    if (current_version->CompareTo(*(extension_->version())) > 0) {
      ReportFailureFromFileThread(
          l10n_util::GetStringUTF8(IDS_EXTENSION_CANT_DOWNGRADE_VERSION));
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
      extension_->creation_flags() | Extension::REQUIRE_KEY,
      &error);
  CHECK(error.empty()) << error;

  ReportSuccessFromFileThread();
}

void CrxInstaller::ReportFailureFromFileThread(const std::string& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&CrxInstaller::ReportFailureFromUIThread, this, error)))
    NOTREACHED();
}

void CrxInstaller::ReportFailureFromUIThread(const std::string& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                  content::Source<CrxInstaller>(this),
                  content::Details<const std::string>(&error));

  // This isn't really necessary, it is only used because unit tests expect to
  // see errors get reported via this interface.
  //
  // TODO(aa): Need to go through unit tests and clean them up too, probably get
  // rid of this line.
  ExtensionErrorReporter::GetInstance()->ReportError(error, false);  // quiet

  if (client_)
    client_->OnInstallFailure(error);

  NotifyCrxInstallComplete(NULL);
}

void CrxInstaller::ReportSuccessFromFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Tracking number of extensions installed by users
  if (install_cause() == extension_misc::INSTALL_CAUSE_USER_DOWNLOAD) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionInstalled", 1, 2);

    static bool default_apps_trial_exists =
        base::FieldTrialList::TrialExists(kDefaultAppsTrialName);
    if (default_apps_trial_exists) {
      UMA_HISTOGRAM_ENUMERATION(
          base::FieldTrial::MakeName("Extensions.ExtensionInstalled",
                                     kDefaultAppsTrialName),
          1, 2);
    }
  }

  if (!BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&CrxInstaller::ReportSuccessFromUIThread, this)))
    NOTREACHED();
}

void CrxInstaller::ReportSuccessFromUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!frontend_weak_.get())
    return;

  // If there is a client, tell the client about installation.
  if (client_)
    client_->OnInstallSuccess(extension_.get(), install_icon_.get());

  // We update the extension's granted permissions if the user already approved
  // the install (client_ is non NULL), or we are allowed to install this
  // silently. We only track granted permissions for INTERNAL extensions.
  if ((client_ || allow_silent_install_) &&
      extension_->location() == Extension::INTERNAL)
    frontend_weak_->GrantPermissions(extension_);

  // Tell the frontend about the installation and hand off ownership of
  // extension_ to it.
  frontend_weak_->OnExtensionInstalled(extension_, is_gallery_install(),
                                       page_index_);

  NotifyCrxInstallComplete(extension_.get());

  extension_ = NULL;

  // We're done. We don't post any more tasks to ourselves so we are deleted
  // soon.
}

void CrxInstaller::NotifyCrxInstallComplete(const Extension* extension) {
  // Some users (such as the download shelf) need to know when a
  // CRXInstaller is done.  Listening for the EXTENSION_* events
  // is problematic because they don't know anything about the
  // extension before it is unpacked, so they cannot filter based
  // on the extension.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CRX_INSTALLER_DONE,
      content::Source<CrxInstaller>(this),
      content::Details<const Extension>(extension));
}
