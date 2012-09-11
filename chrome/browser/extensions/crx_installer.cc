// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/string_util.h"
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
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/requirements_checker.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_switch_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::UserMetricsAction;

namespace extensions {

namespace {

// Used in histograms; do not change order.
enum OffStoreInstallDecision {
  OnStoreInstall,
  OffStoreInstallAllowed,
  OffStoreInstallDisallowed,
  NumOffStoreInstallDecision
};

}  // namespace

// static
scoped_refptr<CrxInstaller> CrxInstaller::Create(
    ExtensionService* frontend,
    ExtensionInstallPrompt* client) {
  return new CrxInstaller(frontend->AsWeakPtr(), client, NULL);
}

// static
scoped_refptr<CrxInstaller> CrxInstaller::Create(
    ExtensionService* frontend,
    ExtensionInstallPrompt* client,
    const WebstoreInstaller::Approval* approval) {
  return new CrxInstaller(frontend->AsWeakPtr(), client, approval);
}

CrxInstaller::CrxInstaller(
    base::WeakPtr<ExtensionService> frontend_weak,
    ExtensionInstallPrompt* client,
    const WebstoreInstaller::Approval* approval)
    : install_directory_(frontend_weak->install_directory()),
      install_source_(Extension::INTERNAL),
      approved_(false),
      extensions_enabled_(frontend_weak->extensions_enabled()),
      delete_source_(false),
      create_app_shortcut_(false),
      frontend_weak_(frontend_weak),
      profile_(frontend_weak->profile()),
      client_(client),
      apps_require_extension_mime_type_(false),
      allow_silent_install_(false),
      install_cause_(extension_misc::INSTALL_CAUSE_UNSET),
      creation_flags_(Extension::NO_FLAGS),
      off_store_install_allow_reason_(OffStoreInstallDisallowed),
      did_handle_successfully_(true),
      record_oauth2_grant_(false),
      error_on_unsupported_requirements_(false),
      requirements_checker_(new extensions::RequirementsChecker()),
      has_requirement_errors_(false) {
  if (!approval)
    return;

  CHECK(profile_->IsSameProfile(approval->profile));
  client_->install_ui()->SetUseAppInstalledBubble(
      approval->use_app_installed_bubble);
  client_->install_ui()->SetSkipPostInstallUI(approval->skip_post_install_ui);

  if (approval->skip_install_dialog) {
    // Mark the extension as approved, but save the expected manifest and ID
    // so we can check that they match the CRX's.
    approved_ = true;
    expected_manifest_.reset(approval->parsed_manifest->DeepCopy());
    expected_id_ = approval->extension_id;
    record_oauth2_grant_ = approval->record_oauth2_grant;
  }
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

  scoped_refptr<SandboxedUnpacker> unpacker(
      new SandboxedUnpacker(
          source_file,
          content::ResourceDispatcherHost::Get() != NULL,
          install_source_,
          creation_flags_,
          this));

  if (!BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&SandboxedUnpacker::Start, unpacker.get())))
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
  string16 error;
  scoped_refptr<Extension> extension = ConvertUserScriptToExtension(
      source_file_, download_url_, &error);
  if (!extension) {
    ReportFailureFromFileThread(CrxInstallerError(error));
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
  string16 error;
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

CrxInstallerError CrxInstaller::AllowInstall(const Extension* extension) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Make sure the expected ID matches if one was supplied or if we want to
  // bypass the prompt.
  if ((approved_ || !expected_id_.empty()) &&
      expected_id_ != extension->id()) {
    return CrxInstallerError(
        l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALL_UNEXPECTED_ID,
                                   ASCIIToUTF16(expected_id_),
                                   ASCIIToUTF16(extension->id())));
  }

  if (expected_version_.get() &&
      !expected_version_->Equals(*extension->version())) {
    return CrxInstallerError(
        l10n_util::GetStringFUTF16(
            IDS_EXTENSION_INSTALL_UNEXPECTED_VERSION,
            ASCIIToUTF16(expected_version_->GetString()),
            ASCIIToUTF16(extension->version()->GetString())));
  }

  // Make sure the manifests match if we want to bypass the prompt.
  if (approved_ &&
      (!expected_manifest_.get() ||
       !expected_manifest_->Equals(original_manifest_.get()))) {
    return CrxInstallerError(
        l10n_util::GetStringUTF16(IDS_EXTENSION_MANIFEST_INVALID));
  }

  // The checks below are skipped for themes and external installs.
  // TODO(pamg): After ManagementPolicy refactoring is complete, remove this
  // and other uses of install_source_ that are no longer needed now that the
  // SandboxedUnpacker sets extension->location.
  if (extension->is_theme() || Extension::IsExternalLocation(install_source_))
    return CrxInstallerError();

  if (!extensions_enabled_) {
    return CrxInstallerError(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_NOT_ENABLED));
  }

  if (install_cause_ == extension_misc::INSTALL_CAUSE_USER_DOWNLOAD) {
    if (switch_utils::IsEasyOffStoreInstallEnabled()) {
      const char* kHistogramName = "Extensions.OffStoreInstallDecisionEasy";
      if (is_gallery_install()) {
        UMA_HISTOGRAM_ENUMERATION(kHistogramName, OnStoreInstall,
                                  NumOffStoreInstallDecision);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kHistogramName, OffStoreInstallAllowed,
                                  NumOffStoreInstallDecision);
      }
    } else {
      const char* kHistogramName = "Extensions.OffStoreInstallDecisionHard";
      if (is_gallery_install()) {
        UMA_HISTOGRAM_ENUMERATION(kHistogramName, OnStoreInstall,
                                  NumOffStoreInstallDecision);
      } else if (off_store_install_allow_reason_ != OffStoreInstallDisallowed) {
        UMA_HISTOGRAM_ENUMERATION(kHistogramName, OffStoreInstallAllowed,
                                  NumOffStoreInstallDecision);
        UMA_HISTOGRAM_ENUMERATION("Extensions.OffStoreInstallAllowReason",
                                  off_store_install_allow_reason_,
                                  NumOffStoreInstallAllowReasons);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kHistogramName, OffStoreInstallDisallowed,
                                  NumOffStoreInstallDecision);
        // Don't delete source in this case so that the user can install
        // manually if they want.
        delete_source_ = false;
        did_handle_successfully_ = false;

        return CrxInstallerError(
            CrxInstallerError::ERROR_OFF_STORE,
            l10n_util::GetStringUTF16(
                IDS_EXTENSION_INSTALL_DISALLOWED_ON_SITE));
      }
    }
  }

  if (extension_->is_app()) {
    // If the app was downloaded, apps_require_extension_mime_type_
    // will be set.  In this case, check that it was served with the
    // right mime type.  Make an exception for file URLs, which come
    // from the users computer and have no headers.
    if (!download_url_.SchemeIsFile() &&
        apps_require_extension_mime_type_ &&
        original_mime_type_ != Extension::kMimeType) {
      return CrxInstallerError(
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_INSTALL_INCORRECT_APP_CONTENT_TYPE,
              ASCIIToUTF16(Extension::kMimeType)));
    }

    // If the client_ is NULL, then the app is either being installed via
    // an internal mechanism like sync, external_extensions, or default apps.
    // In that case, we don't want to enforce things like the install origin.
    if (!is_gallery_install() && client_) {
      // For apps with a gallery update URL, require that they be installed
      // from the gallery.
      // TODO(erikkay) Apply this rule for paid extensions and themes as well.
      if (extension->UpdatesFromGallery()) {
        return CrxInstallerError(
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_DISALLOW_NON_DOWNLOADED_GALLERY_INSTALLS,
                l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)));
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
          return CrxInstallerError(
              l10n_util::GetStringUTF16(
                  IDS_EXTENSION_INSTALL_INCORRECT_INSTALL_HOST));
        }
      }
    }
  }

  return CrxInstallerError();
}

void CrxInstaller::OnUnpackFailure(const string16& error_message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  UMA_HISTOGRAM_ENUMERATION("Extensions.UnpackFailureInstallSource",
                            install_source(), Extension::NUM_LOCATIONS);

  UMA_HISTOGRAM_ENUMERATION("Extensions.UnpackFailureInstallCause",
                            install_cause(),
                            extension_misc::NUM_INSTALL_CAUSES);

  ReportFailureFromFileThread(CrxInstallerError(error_message));
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

  CrxInstallerError error = AllowInstall(extension);
  if (error.type() != CrxInstallerError::ERROR_NONE) {
    ReportFailureFromFileThread(error);
    return;
  }

  if (client_) {
    Extension::DecodeIcon(extension_.get(),
                          extension_misc::EXTENSION_ICON_LARGE,
                          ExtensionIconSet::MATCH_BIGGER,
                          &install_icon_);
  }

  if (!BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&CrxInstaller::CheckRequirements, this)))
    NOTREACHED();
}

void CrxInstaller::CheckRequirements() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AddRef();  // Balanced in OnRequirementsChecked().
  requirements_checker_->Check(extension_,
                               base::Bind(&CrxInstaller::OnRequirementsChecked,
                                          this));
}

void CrxInstaller::OnRequirementsChecked(
    std::vector<std::string> requirement_errors) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Release();  // Balanced in CheckRequirements().
  if (!requirement_errors.empty()) {
    if (error_on_unsupported_requirements_) {
      ReportFailureFromUIThread(CrxInstallerError(
          UTF8ToUTF16(JoinString(requirement_errors, ' '))));
      return;
    }
    has_requirement_errors_ = true;
  }

  ConfirmInstall();
}

void CrxInstaller::ConfirmInstall() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!frontend_weak_.get())
    return;

  string16 error;
  if (!ExtensionSystem::Get(profile_)->management_policy()->
      UserMayLoad(extension_, &error)) {
    ReportFailureFromUIThread(CrxInstallerError(error));
    return;
  }

  GURL overlapping_url;
  const Extension* overlapping_extension =
      frontend_weak_->extensions()->
      GetHostedAppByOverlappingWebExtent(extension_->web_extent());
  if (overlapping_extension &&
      overlapping_extension->id() != extension_->id()) {
    ReportFailureFromUIThread(
        CrxInstallerError(
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_OVERLAPPING_WEB_EXTENT,
                UTF8ToUTF16(overlapping_extension->name()))));
    return;
  }

  current_version_ =
      frontend_weak_->extension_prefs()->GetVersionString(extension_->id());

  if (client_ && (!allow_silent_install_ || !approved_)) {
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

  NotifyCrxInstallComplete(NULL);

  Release();  // balanced in ConfirmInstall().

  // We're done. Since we don't post any more tasks to ourself, our ref count
  // should go to zero and we die. The destructor will clean up the temp dir.
}

void CrxInstaller::CompleteInstall() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!current_version_.empty()) {
    Version current_version(current_version_);
    if (current_version.CompareTo(*(extension_->version())) > 0) {
      ReportFailureFromFileThread(
          CrxInstallerError(
              l10n_util::GetStringUTF16(IDS_EXTENSION_CANT_DOWNGRADE_VERSION)));
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
        CrxInstallerError(
            l10n_util::GetStringUTF16(
                IDS_EXTENSION_MOVE_DIRECTORY_TO_PROFILE_FAILED)));
    return;
  }

  // This is lame, but we must reload the extension because absolute paths
  // inside the content scripts are established inside InitFromValue() and we
  // just moved the extension.
  // TODO(aa): All paths to resources inside extensions should be created
  // lazily and based on the Extension's root path at that moment.
  // TODO(rdevlin.cronin): Continue removing std::string errors and replacing
  // with string16
  std::string extension_id = extension_->id();
  std::string error;
  extension_ = extension_file_util::LoadExtension(
      version_dir,
      install_source_,
      extension_->creation_flags() | Extension::REQUIRE_KEY,
      &error);

  if (extension_) {
    ReportSuccessFromFileThread();
  } else {
    LOG(ERROR) << error << " " << extension_id << " " << download_url_.spec();
    ReportFailureFromFileThread(CrxInstallerError(UTF8ToUTF16(error)));
  }

}

void CrxInstaller::ReportFailureFromFileThread(const CrxInstallerError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&CrxInstaller::ReportFailureFromUIThread, this, error))) {
    NOTREACHED();
  }
}

void CrxInstaller::ReportFailureFromUIThread(const CrxInstallerError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                  content::Source<CrxInstaller>(this),
                  content::Details<const string16>(&error.message()));

  // This isn't really necessary, it is only used because unit tests expect to
  // see errors get reported via this interface.
  //
  // TODO(aa): Need to go through unit tests and clean them up too, probably get
  // rid of this line.
  ExtensionErrorReporter::GetInstance()->ReportError(
      error.message(), false);  // quiet

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
  if (client_) {
    client_->OnInstallSuccess(extension_.get(), install_icon_.get());
  }

  if (client_ && !approved_)
    record_oauth2_grant_ = client_->record_oauth2_grant();

  // We update the extension's granted permissions if the user already approved
  // the install (client_ is non NULL), or we are allowed to install this
  // silently.
  if (client_ || allow_silent_install_) {
    PermissionsUpdater perms_updater(profile());
    perms_updater.GrantActivePermissions(extension_, record_oauth2_grant_);
  }

  // Tell the frontend about the installation and hand off ownership of
  // extension_ to it.
  frontend_weak_->OnExtensionInstalled(extension_,
                                       is_gallery_install(),
                                       page_ordinal_,
                                       has_requirement_errors_);

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

}  // namespace extensions
