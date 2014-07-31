// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/extensions/convert_user_script.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/crx_installer_error.h"
#include "chrome/browser/extensions/extension_assets_manager.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"
#include "grit/chromium_strings.h"
#include "grit/extensions_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/user_manager.h"
#endif

using base::UserMetricsAction;
using content::BrowserThread;
using extensions::SharedModuleInfo;

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
scoped_refptr<CrxInstaller> CrxInstaller::CreateSilent(
    ExtensionService* frontend) {
  return new CrxInstaller(frontend->AsWeakPtr(),
                          scoped_ptr<ExtensionInstallPrompt>(),
                          NULL);
}

// static
scoped_refptr<CrxInstaller> CrxInstaller::Create(
    ExtensionService* frontend,
    scoped_ptr<ExtensionInstallPrompt> client) {
  return new CrxInstaller(frontend->AsWeakPtr(), client.Pass(), NULL);
}

// static
scoped_refptr<CrxInstaller> CrxInstaller::Create(
    ExtensionService* service,
    scoped_ptr<ExtensionInstallPrompt> client,
    const WebstoreInstaller::Approval* approval) {
  return new CrxInstaller(service->AsWeakPtr(), client.Pass(), approval);
}

CrxInstaller::CrxInstaller(base::WeakPtr<ExtensionService> service_weak,
                           scoped_ptr<ExtensionInstallPrompt> client,
                           const WebstoreInstaller::Approval* approval)
    : install_directory_(service_weak->install_directory()),
      install_source_(Manifest::INTERNAL),
      approved_(false),
      expected_manifest_check_level_(
          WebstoreInstaller::MANIFEST_CHECK_LEVEL_STRICT),
      expected_version_strict_checking_(false),
      extensions_enabled_(service_weak->extensions_enabled()),
      delete_source_(false),
      create_app_shortcut_(false),
      service_weak_(service_weak),
      // See header file comment on |client_| for why we use a raw pointer here.
      client_(client.release()),
      apps_require_extension_mime_type_(false),
      allow_silent_install_(false),
      grant_permissions_(true),
      install_cause_(extension_misc::INSTALL_CAUSE_UNSET),
      creation_flags_(Extension::NO_FLAGS),
      off_store_install_allow_reason_(OffStoreInstallDisallowed),
      did_handle_successfully_(true),
      error_on_unsupported_requirements_(false),
      update_from_settings_page_(false),
      install_flags_(kInstallFlagNone),
      install_checker_(service_weak->profile()) {
  installer_task_runner_ = service_weak->GetFileTaskRunner();
  if (!approval)
    return;

  CHECK(profile()->IsSameProfile(approval->profile));
  if (client_) {
    client_->install_ui()->SetUseAppInstalledBubble(
        approval->use_app_installed_bubble);
    client_->install_ui()->set_skip_post_install_ui(
        approval->skip_post_install_ui);
  }

  if (approval->skip_install_dialog) {
    // Mark the extension as approved, but save the expected manifest and ID
    // so we can check that they match the CRX's.
    approved_ = true;
    expected_manifest_check_level_ = approval->manifest_check_level;
    if (expected_manifest_check_level_ !=
        WebstoreInstaller::MANIFEST_CHECK_LEVEL_NONE)
      expected_manifest_.reset(approval->manifest->DeepCopy());
    expected_id_ = approval->extension_id;
  }
  if (approval->minimum_version.get()) {
    expected_version_.reset(new Version(*approval->minimum_version));
    expected_version_strict_checking_ = false;
  }

  show_dialog_callback_ = approval->show_dialog_callback;
  set_is_ephemeral(approval->is_ephemeral);
}

CrxInstaller::~CrxInstaller() {
  // Make sure the UI is deleted on the ui thread.
  if (client_) {
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, client_);
    client_ = NULL;
  }
}

void CrxInstaller::InstallCrx(const base::FilePath& source_file) {
  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  NotifyCrxInstallBegin();

  source_file_ = source_file;

  scoped_refptr<SandboxedUnpacker> unpacker(
      new SandboxedUnpacker(source_file,
                            install_source_,
                            creation_flags_,
                            install_directory_,
                            installer_task_runner_.get(),
                            this));

  if (!installer_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&SandboxedUnpacker::Start, unpacker.get())))
    NOTREACHED();
}

void CrxInstaller::InstallUserScript(const base::FilePath& source_file,
                                     const GURL& download_url) {
  DCHECK(!download_url.is_empty());

  NotifyCrxInstallBegin();

  source_file_ = source_file;
  download_url_ = download_url;

  if (!installer_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&CrxInstaller::ConvertUserScriptOnFileThread, this)))
    NOTREACHED();
}

void CrxInstaller::ConvertUserScriptOnFileThread() {
  base::string16 error;
  scoped_refptr<Extension> extension = ConvertUserScriptToExtension(
      source_file_, download_url_, install_directory_, &error);
  if (!extension.get()) {
    ReportFailureFromFileThread(CrxInstallerError(error));
    return;
  }

  OnUnpackSuccess(extension->path(), extension->path(), NULL, extension.get(),
                  SkBitmap());
}

void CrxInstaller::InstallWebApp(const WebApplicationInfo& web_app) {
  NotifyCrxInstallBegin();

  if (!installer_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&CrxInstaller::ConvertWebAppOnFileThread, this, web_app)))
    NOTREACHED();
}

void CrxInstaller::ConvertWebAppOnFileThread(
    const WebApplicationInfo& web_app) {
  scoped_refptr<Extension> extension(ConvertWebAppToExtension(
      web_app, base::Time::Now(), install_directory_));
  if (!extension.get()) {
    // Validation should have stopped any potential errors before getting here.
    NOTREACHED() << "Could not convert web app to extension.";
    return;
  }

  // TODO(aa): conversion data gets lost here :(

  OnUnpackSuccess(extension->path(), extension->path(), NULL, extension.get(),
                  SkBitmap());
}

CrxInstallerError CrxInstaller::AllowInstall(const Extension* extension) {
  DCHECK(installer_task_runner_->RunsTasksOnCurrentThread());

  // Make sure the expected ID matches if one was supplied or if we want to
  // bypass the prompt.
  if ((approved_ || !expected_id_.empty()) &&
      expected_id_ != extension->id()) {
    return CrxInstallerError(
        l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALL_UNEXPECTED_ID,
                                   base::ASCIIToUTF16(expected_id_),
                                   base::ASCIIToUTF16(extension->id())));
  }

  if (expected_version_.get()) {
    if (expected_version_strict_checking_) {
      if (!expected_version_->Equals(*extension->version())) {
        return CrxInstallerError(
            l10n_util::GetStringFUTF16(
              IDS_EXTENSION_INSTALL_UNEXPECTED_VERSION,
              base::ASCIIToUTF16(expected_version_->GetString()),
              base::ASCIIToUTF16(extension->version()->GetString())));
      }
    } else {
      if (extension->version()->CompareTo(*expected_version_) < 0) {
        return CrxInstallerError(
            l10n_util::GetStringFUTF16(
              IDS_EXTENSION_INSTALL_UNEXPECTED_VERSION,
              base::ASCIIToUTF16(expected_version_->GetString() + "+"),
              base::ASCIIToUTF16(extension->version()->GetString())));
      }
    }
  }

  // Make sure the manifests match if we want to bypass the prompt.
  if (approved_) {
    bool valid = false;
    if (expected_manifest_check_level_ ==
        WebstoreInstaller::MANIFEST_CHECK_LEVEL_NONE) {
        // To skip manifest checking, the extension must be a shared module
        // and not request any permissions.
      if (SharedModuleInfo::IsSharedModule(extension) &&
          extension->permissions_data()->active_permissions()->IsEmpty()) {
          valid = true;
        }
    } else {
      valid = expected_manifest_->Equals(original_manifest_.get());
      if (!valid && expected_manifest_check_level_ ==
          WebstoreInstaller::MANIFEST_CHECK_LEVEL_LOOSE) {
        std::string error;
        scoped_refptr<Extension> dummy_extension =
            Extension::Create(base::FilePath(),
                              install_source_,
                              *expected_manifest_->value(),
                              creation_flags_,
                              &error);
        if (error.empty()) {
          scoped_refptr<const PermissionSet> expected_permissions =
              dummy_extension->permissions_data()->active_permissions();
          valid = !(PermissionMessageProvider::Get()->IsPrivilegeIncrease(
              expected_permissions,
              extension->permissions_data()->active_permissions(),
              extension->GetType()));
        }
      }
    }

    if (!valid)
      return CrxInstallerError(
          l10n_util::GetStringUTF16(IDS_EXTENSION_MANIFEST_INVALID));
  }

  // The checks below are skipped for themes and external installs.
  // TODO(pamg): After ManagementPolicy refactoring is complete, remove this
  // and other uses of install_source_ that are no longer needed now that the
  // SandboxedUnpacker sets extension->location.
  if (extension->is_theme() || Manifest::IsExternalLocation(install_source_))
    return CrxInstallerError();

  if (!extensions_enabled_) {
    return CrxInstallerError(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALL_NOT_ENABLED));
  }

  if (install_cause_ == extension_misc::INSTALL_CAUSE_USER_DOWNLOAD) {
    if (FeatureSwitch::easy_off_store_install()->IsEnabled()) {
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

  if (install_checker_.extension()->is_app()) {
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
              base::ASCIIToUTF16(Extension::kMimeType)));
    }

    // If the client_ is NULL, then the app is either being installed via
    // an internal mechanism like sync, external_extensions, or default apps.
    // In that case, we don't want to enforce things like the install origin.
    if (!is_gallery_install() && client_) {
      // For apps with a gallery update URL, require that they be installed
      // from the gallery.
      // TODO(erikkay) Apply this rule for paid extensions and themes as well.
      if (ManifestURL::UpdatesFromGallery(extension)) {
        return CrxInstallerError(
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_DISALLOW_NON_DOWNLOADED_GALLERY_INSTALLS,
                l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)));
      }

      // For self-hosted apps, verify that the entire extent is on the same
      // host (or a subdomain of the host) the download happened from.  There's
      // no way for us to verify that the app controls any other hosts.
      URLPattern pattern(UserScript::ValidUserScriptSchemes());
      pattern.SetHost(download_url_.host());
      pattern.SetMatchSubdomains(true);

      URLPatternSet patterns = install_checker_.extension()->web_extent();
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

void CrxInstaller::OnUnpackFailure(const base::string16& error_message) {
  DCHECK(installer_task_runner_->RunsTasksOnCurrentThread());

  UMA_HISTOGRAM_ENUMERATION("Extensions.UnpackFailureInstallSource",
                            install_source(), Manifest::NUM_LOCATIONS);

  UMA_HISTOGRAM_ENUMERATION("Extensions.UnpackFailureInstallCause",
                            install_cause(),
                            extension_misc::NUM_INSTALL_CAUSES);

  ReportFailureFromFileThread(CrxInstallerError(error_message));
}

void CrxInstaller::OnUnpackSuccess(
    const base::FilePath& temp_dir,
    const base::FilePath& extension_dir,
    const base::DictionaryValue* original_manifest,
    const Extension* extension,
    const SkBitmap& install_icon) {
  DCHECK(installer_task_runner_->RunsTasksOnCurrentThread());

  UMA_HISTOGRAM_ENUMERATION("Extensions.UnpackSuccessInstallSource",
                            install_source(), Manifest::NUM_LOCATIONS);


  UMA_HISTOGRAM_ENUMERATION("Extensions.UnpackSuccessInstallCause",
                            install_cause(),
                            extension_misc::NUM_INSTALL_CAUSES);

  install_checker_.set_extension(extension);
  temp_dir_ = temp_dir;
  if (!install_icon.empty())
    install_icon_.reset(new SkBitmap(install_icon));

  if (original_manifest)
    original_manifest_.reset(new Manifest(
        Manifest::INVALID_LOCATION,
        scoped_ptr<base::DictionaryValue>(original_manifest->DeepCopy())));

  // We don't have to delete the unpack dir explicity since it is a child of
  // the temp dir.
  unpacked_extension_root_ = extension_dir;

  CrxInstallerError error = AllowInstall(extension);
  if (error.type() != CrxInstallerError::ERROR_NONE) {
    ReportFailureFromFileThread(error);
    return;
  }

  if (!BrowserThread::PostTask(BrowserThread::UI,
                               FROM_HERE,
                               base::Bind(&CrxInstaller::CheckInstall, this)))
    NOTREACHED();
}

void CrxInstaller::CheckInstall() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  if (SharedModuleInfo::ImportsModules(extension())) {
    const std::vector<SharedModuleInfo::ImportInfo>& imports =
        SharedModuleInfo::GetImports(extension());
    std::vector<SharedModuleInfo::ImportInfo>::const_iterator i;
    for (i = imports.begin(); i != imports.end(); ++i) {
      const Extension* imported_module =
          service->GetExtensionById(i->extension_id, true);
      if (imported_module &&
          !SharedModuleInfo::IsSharedModule(imported_module)) {
        ReportFailureFromUIThread(
            CrxInstallerError(l10n_util::GetStringFUTF16(
                IDS_EXTENSION_INSTALL_DEPENDENCY_NOT_SHARED_MODULE,
                base::ASCIIToUTF16(i->extension_id))));
        return;
      } else if (imported_module &&
          !SharedModuleInfo::IsExportAllowedByWhitelist(imported_module,
                                                        extension()->id())) {
        ReportFailureFromUIThread(
            CrxInstallerError(l10n_util::GetStringFUTF16(
                IDS_EXTENSION_INSTALL_DEPENDENCY_NOT_WHITELISTED,
                base::ASCIIToUTF16(i->extension_id))));
        return;
      }
    }
  }

  // Run the policy, requirements and blacklist checks in parallel.
  install_checker_.Start(
      ExtensionInstallChecker::CHECK_ALL,
      false /* fail fast */,
      base::Bind(&CrxInstaller::OnInstallChecksComplete, this));
}

void CrxInstaller::OnInstallChecksComplete(int failed_checks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_weak_)
    return;

  // Check for requirement errors.
  if (!install_checker_.requirement_errors().empty()) {
    if (error_on_unsupported_requirements_) {
      ReportFailureFromUIThread(CrxInstallerError(base::UTF8ToUTF16(
          JoinString(install_checker_.requirement_errors(), ' '))));
      return;
    }
    install_flags_ |= kInstallFlagHasRequirementErrors;
  }

  // Check the blacklist state.
  if (install_checker_.blacklist_state() == extensions::BLACKLISTED_MALWARE) {
    install_flags_ |= kInstallFlagIsBlacklistedForMalware;
  }

  if ((install_checker_.blacklist_state() == extensions::BLACKLISTED_MALWARE ||
       install_checker_.blacklist_state() == extensions::BLACKLISTED_UNKNOWN) &&
      !allow_silent_install_) {
    // User tried to install a blacklisted extension. Show an error and
    // refuse to install it.
    ReportFailureFromUIThread(extensions::CrxInstallerError(
        l10n_util::GetStringFUTF16(IDS_EXTENSION_IS_BLACKLISTED,
                                   base::UTF8ToUTF16(extension()->name()))));
    UMA_HISTOGRAM_ENUMERATION("ExtensionBlacklist.BlockCRX",
                              extension()->location(),
                              Manifest::NUM_LOCATIONS);
    return;
  }

  // NOTE: extension may still be blacklisted, but we're forced to silently
  // install it. In this case, ExtensionService::OnExtensionInstalled needs to
  // deal with it.

  // Check for policy errors.
  if (!install_checker_.policy_error().empty()) {
    // We don't want to show the error infobar for installs from the WebStore,
    // because the WebStore already shows an error dialog itself.
    // Note: |client_| can be NULL in unit_tests!
    if (extension()->from_webstore() && client_)
      client_->install_ui()->set_skip_post_install_ui(true);
    ReportFailureFromUIThread(
        CrxInstallerError(base::UTF8ToUTF16(install_checker_.policy_error())));
    return;
  }

  ConfirmInstall();
}

void CrxInstaller::ConfirmInstall() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  if (KioskModeInfo::IsKioskOnly(install_checker_.extension())) {
    bool in_kiosk_mode = false;
#if defined(OS_CHROMEOS)
    chromeos::UserManager* user_manager = chromeos::UserManager::Get();
    in_kiosk_mode = user_manager && user_manager->IsLoggedInAsKioskApp();
#endif
    if (!in_kiosk_mode) {
      ReportFailureFromUIThread(CrxInstallerError(
          l10n_util::GetStringUTF16(
              IDS_EXTENSION_INSTALL_KIOSK_MODE_ONLY)));
      return;
    }
  }

  // Check whether this install is initiated from the settings page to
  // update an existing extension or app.
  CheckUpdateFromSettingsPage();

  GURL overlapping_url;
  const Extension* overlapping_extension =
      service->extensions()->GetHostedAppByOverlappingWebExtent(
          extension()->web_extent());
  if (overlapping_extension &&
      overlapping_extension->id() != extension()->id()) {
    ReportFailureFromUIThread(
        CrxInstallerError(
            l10n_util::GetStringFUTF16(
                IDS_EXTENSION_OVERLAPPING_WEB_EXTENT,
                base::UTF8ToUTF16(overlapping_extension->name()))));
    return;
  }

  current_version_ = ExtensionPrefs::Get(service->profile())
                         ->GetVersionString(extension()->id());

  if (client_ &&
      (!allow_silent_install_ || !approved_) &&
      !update_from_settings_page_) {
    AddRef();  // Balanced in InstallUIProceed() and InstallUIAbort().
    client_->ConfirmInstall(this, extension(), show_dialog_callback_);
  } else {
    if (!installer_task_runner_->PostTask(
            FROM_HERE,
            base::Bind(&CrxInstaller::CompleteInstall, this)))
      NOTREACHED();
  }
  return;
}

void CrxInstaller::InstallUIProceed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  // If update_from_settings_page_ boolean is true, this functions is
  // getting called in response to ExtensionInstallPrompt::ConfirmReEnable()
  // and if it is false, this function is called in response to
  // ExtensionInstallPrompt::ConfirmInstall().
  if (update_from_settings_page_) {
    service->GrantPermissionsAndEnableExtension(extension());
  } else {
    if (!installer_task_runner_->PostTask(
            FROM_HERE,
            base::Bind(&CrxInstaller::CompleteInstall, this)))
      NOTREACHED();
  }

  Release();  // balanced in ConfirmInstall() or ConfirmReEnable().
}

void CrxInstaller::InstallUIAbort(bool user_initiated) {
  // If update_from_settings_page_ boolean is true, this functions is
  // getting called in response to ExtensionInstallPrompt::ConfirmReEnable()
  // and if it is false, this function is called in response to
  // ExtensionInstallPrompt::ConfirmInstall().
  if (!update_from_settings_page_) {
    std::string histogram_name = user_initiated
                                     ? "Extensions.Permissions_InstallCancel2"
                                     : "Extensions.Permissions_InstallAbort2";
    ExtensionService::RecordPermissionMessagesHistogram(
        extension(), histogram_name.c_str());

    NotifyCrxInstallComplete(false);
  }

  Release();  // balanced in ConfirmInstall() or ConfirmReEnable().

  // We're done. Since we don't post any more tasks to ourself, our ref count
  // should go to zero and we die. The destructor will clean up the temp dir.
}

void CrxInstaller::CompleteInstall() {
  DCHECK(installer_task_runner_->RunsTasksOnCurrentThread());

  if (!current_version_.empty()) {
    Version current_version(current_version_);
    if (current_version.CompareTo(*(extension()->version())) > 0) {
      ReportFailureFromFileThread(
          CrxInstallerError(
              l10n_util::GetStringUTF16(extension()->is_app() ?
                  IDS_APP_CANT_DOWNGRADE_VERSION :
                  IDS_EXTENSION_CANT_DOWNGRADE_VERSION)));
      return;
    }
  }

  // See how long extension install paths are.  This is important on
  // windows, because file operations may fail if the path to a file
  // exceeds a small constant.  See crbug.com/69693 .
  UMA_HISTOGRAM_CUSTOM_COUNTS(
    "Extensions.CrxInstallDirPathLength",
        install_directory_.value().length(), 0, 500, 100);

  ExtensionAssetsManager* assets_manager =
      ExtensionAssetsManager::GetInstance();
  assets_manager->InstallExtension(
      extension(),
      unpacked_extension_root_,
      install_directory_,
      profile(),
      base::Bind(&CrxInstaller::ReloadExtensionAfterInstall, this));
}

void CrxInstaller::ReloadExtensionAfterInstall(
    const base::FilePath& version_dir) {
  DCHECK(installer_task_runner_->RunsTasksOnCurrentThread());

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
  // with base::string16
  std::string extension_id = extension()->id();
  std::string error;
  install_checker_.set_extension(
      file_util::LoadExtension(
          version_dir,
          install_source_,
          extension()->creation_flags() | Extension::REQUIRE_KEY,
          &error).get());

  if (extension()) {
    ReportSuccessFromFileThread();
  } else {
    LOG(ERROR) << error << " " << extension_id << " " << download_url_;
    ReportFailureFromFileThread(CrxInstallerError(base::UTF8ToUTF16(error)));
  }
}

void CrxInstaller::ReportFailureFromFileThread(const CrxInstallerError& error) {
  DCHECK(installer_task_runner_->RunsTasksOnCurrentThread());
  if (!BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&CrxInstaller::ReportFailureFromUIThread, this, error))) {
    NOTREACHED();
  }
}

void CrxInstaller::ReportFailureFromUIThread(const CrxInstallerError& error) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!service_weak_.get() || service_weak_->browser_terminating())
    return;

  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                  content::Source<CrxInstaller>(this),
                  content::Details<const base::string16>(&error.message()));

  // This isn't really necessary, it is only used because unit tests expect to
  // see errors get reported via this interface.
  //
  // TODO(aa): Need to go through unit tests and clean them up too, probably get
  // rid of this line.
  ExtensionErrorReporter::GetInstance()->ReportError(
      error.message(),
      false);  // Be quiet.

  if (client_)
    client_->OnInstallFailure(error);

  NotifyCrxInstallComplete(false);

  // Delete temporary files.
  CleanupTempFiles();
}

void CrxInstaller::ReportSuccessFromFileThread() {
  DCHECK(installer_task_runner_->RunsTasksOnCurrentThread());

  // Tracking number of extensions installed by users
  if (install_cause() == extension_misc::INSTALL_CAUSE_USER_DOWNLOAD)
    UMA_HISTOGRAM_ENUMERATION("Extensions.ExtensionInstalled", 1, 2);

  if (!BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&CrxInstaller::ReportSuccessFromUIThread, this)))
    NOTREACHED();

  // Delete temporary files.
  CleanupTempFiles();
}

void CrxInstaller::ReportSuccessFromUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!service_weak_.get() || service_weak_->browser_terminating())
    return;

  if (!update_from_settings_page_) {
    // If there is a client, tell the client about installation.
    if (client_)
      client_->OnInstallSuccess(extension(), install_icon_.get());

    // We update the extension's granted permissions if the user already
    // approved the install (client_ is non NULL), or we are allowed to install
    // this silently.
    if ((client_ || allow_silent_install_) && grant_permissions_) {
      PermissionsUpdater perms_updater(profile());
      perms_updater.GrantActivePermissions(extension());
    }
  }

  service_weak_->OnExtensionInstalled(
      extension(), page_ordinal_, install_flags_);
  NotifyCrxInstallComplete(true);
}

void CrxInstaller::NotifyCrxInstallBegin() {
  InstallTrackerFactory::GetForBrowserContext(profile())
      ->OnBeginCrxInstall(expected_id_);
}

void CrxInstaller::NotifyCrxInstallComplete(bool success) {
  // Some users (such as the download shelf) need to know when a
  // CRXInstaller is done.  Listening for the EXTENSION_* events
  // is problematic because they don't know anything about the
  // extension before it is unpacked, so they cannot filter based
  // on the extension.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::Source<CrxInstaller>(this),
      content::Details<const Extension>(success ? extension() : NULL));

  InstallTrackerFactory::GetForBrowserContext(profile())
      ->OnFinishCrxInstall(success ? extension()->id() : expected_id_, success);

  if (success)
    ConfirmReEnable();
}

void CrxInstaller::CleanupTempFiles() {
  if (!installer_task_runner_->RunsTasksOnCurrentThread()) {
    if (!installer_task_runner_->PostTask(
            FROM_HERE,
            base::Bind(&CrxInstaller::CleanupTempFiles, this))) {
      NOTREACHED();
    }
    return;
  }

  // Delete the temp directory and crx file as necessary.
  if (!temp_dir_.value().empty()) {
    file_util::DeleteFile(temp_dir_, true);
    temp_dir_ = base::FilePath();
  }

  if (delete_source_ && !source_file_.value().empty()) {
    file_util::DeleteFile(source_file_, false);
    source_file_ = base::FilePath();
  }
}

void CrxInstaller::CheckUpdateFromSettingsPage() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  if (off_store_install_allow_reason_ != OffStoreInstallAllowedFromSettingsPage)
    return;

  const Extension* installed_extension =
      service->GetInstalledExtension(extension()->id());
  if (installed_extension) {
    // Previous version of the extension exists.
    update_from_settings_page_ = true;
    expected_id_ = installed_extension->id();
    install_source_ = installed_extension->location();
    install_cause_ = extension_misc::INSTALL_CAUSE_UPDATE;
  }
}

void CrxInstaller::ConfirmReEnable() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ExtensionService* service = service_weak_.get();
  if (!service || service->browser_terminating())
    return;

  if (!update_from_settings_page_)
    return;

  ExtensionPrefs* prefs = ExtensionPrefs::Get(service->profile());
  if (!prefs->DidExtensionEscalatePermissions(extension()->id()))
    return;

  if (client_) {
    AddRef();  // Balanced in InstallUIProceed() and InstallUIAbort().
    client_->ConfirmReEnable(this, extension());
  }
}

}  // namespace extensions
