// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_installer.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/shared_module_info.h"
#include "chrome/common/omaha_query_params/omaha_query_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/manifest_constants.h"
#include "net/base/escape.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/file_system_util.h"
#endif

using chrome::OmahaQueryParams;
using content::BrowserContext;
using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;
using content::NavigationController;
using content::DownloadUrlParameters;

namespace {

// Key used to attach the Approval to the DownloadItem.
const char kApprovalKey[] = "extensions.webstore_installer";

const char kInvalidIdError[] = "Invalid id";
const char kDownloadDirectoryError[] = "Could not create download directory";
const char kDownloadCanceledError[] = "Download canceled";
const char kInstallCanceledError[] = "Install canceled";
const char kDownloadInterruptedError[] = "Download interrupted";
const char kInvalidDownloadError[] =
    "Download was not a valid extension or user script";
const char kDependencyNotFoundError[] = "Dependency not found";
const char kDependencyNotSharedModuleError[] =
  "Dependency is not shared module";
const char kInlineInstallSource[] = "inline";
const char kDefaultInstallSource[] = "ondemand";
const char kAppLauncherInstallSource[] = "applauncher";

base::FilePath* g_download_directory_for_tests = NULL;

// Must be executed on the FILE thread.
void GetDownloadFilePath(
    const base::FilePath& download_directory, const std::string& id,
    const base::Callback<void(const base::FilePath&)>& callback) {
  base::FilePath directory(g_download_directory_for_tests ?
                     *g_download_directory_for_tests : download_directory);

#if defined(OS_CHROMEOS)
  // Do not use drive for extension downloads.
  if (drive::util::IsUnderDriveMountPoint(directory))
    directory = DownloadPrefs::GetDefaultDownloadDirectory();
#endif

  // Ensure the download directory exists. TODO(asargent) - make this use
  // common code from the downloads system.
  if (!base::DirectoryExists(directory)) {
    if (!file_util::CreateDirectory(directory)) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::Bind(callback, base::FilePath()));
      return;
    }
  }

  // This is to help avoid a race condition between when we generate this
  // filename and when the download starts writing to it (think concurrently
  // running sharded browser tests installing the same test file, for
  // instance).
  std::string random_number =
      base::Uint64ToString(base::RandGenerator(kuint16max));

  base::FilePath file =
      directory.AppendASCII(id + "_" + random_number + ".crx");

  int uniquifier =
      file_util::GetUniquePathNumber(file, base::FilePath::StringType());
  if (uniquifier > 0) {
    file = file.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", uniquifier));
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, file));
}

}  // namespace

namespace extensions {

// static
GURL WebstoreInstaller::GetWebstoreInstallURL(
    const std::string& extension_id, InstallSource source) {
  std::string install_source;
  switch (source) {
    case INSTALL_SOURCE_INLINE:
      install_source = kInlineInstallSource;
      break;
    case INSTALL_SOURCE_APP_LAUNCHER:
      install_source = kAppLauncherInstallSource;
      break;
    case INSTALL_SOURCE_OTHER:
      install_source = kDefaultInstallSource;
  }

  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kAppsGalleryDownloadURL)) {
    std::string download_url =
        cmd_line->GetSwitchValueASCII(switches::kAppsGalleryDownloadURL);
    return GURL(base::StringPrintf(download_url.c_str(),
                                   extension_id.c_str()));
  }
  std::vector<std::string> params;
  params.push_back("id=" + extension_id);
  if (!install_source.empty())
    params.push_back("installsource=" + install_source);
  params.push_back("lang=" + g_browser_process->GetApplicationLocale());
  params.push_back("uc");
  std::string url_string = extension_urls::GetWebstoreUpdateUrl().spec();

  GURL url(url_string + "?response=redirect&" +
           OmahaQueryParams::Get(OmahaQueryParams::CRX) + "&x=" +
           net::EscapeQueryParamValue(JoinString(params, '&'), true));
  DCHECK(url.is_valid());

  return url;
}

void WebstoreInstaller::Delegate::OnExtensionDownloadStarted(
    const std::string& id,
    content::DownloadItem* item) {
}

void WebstoreInstaller::Delegate::OnExtensionDownloadProgress(
    const std::string& id,
    content::DownloadItem* item) {
}

WebstoreInstaller::Approval::Approval()
    : profile(NULL),
      use_app_installed_bubble(false),
      skip_post_install_ui(false),
      skip_install_dialog(false),
      enable_launcher(false),
      manifest_check_level(MANIFEST_CHECK_LEVEL_STRICT) {
}

scoped_ptr<WebstoreInstaller::Approval>
WebstoreInstaller::Approval::CreateWithInstallPrompt(Profile* profile) {
  scoped_ptr<Approval> result(new Approval());
  result->profile = profile;
  return result.Pass();
}

scoped_ptr<WebstoreInstaller::Approval>
WebstoreInstaller::Approval::CreateForSharedModule(Profile* profile) {
  scoped_ptr<Approval> result(new Approval());
  result->profile = profile;
  result->skip_install_dialog = true;
  result->manifest_check_level = MANIFEST_CHECK_LEVEL_NONE;
  return result.Pass();
}

scoped_ptr<WebstoreInstaller::Approval>
WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
    Profile* profile,
    const std::string& extension_id,
    scoped_ptr<base::DictionaryValue> parsed_manifest,
    bool strict_manifest_check) {
  scoped_ptr<Approval> result(new Approval());
  result->extension_id = extension_id;
  result->profile = profile;
  result->manifest = scoped_ptr<Manifest>(
      new Manifest(Manifest::INVALID_LOCATION,
                   scoped_ptr<DictionaryValue>(parsed_manifest->DeepCopy())));
  result->skip_install_dialog = true;
  result->manifest_check_level = strict_manifest_check ?
    MANIFEST_CHECK_LEVEL_STRICT : MANIFEST_CHECK_LEVEL_LOOSE;
  return result.Pass();
}

WebstoreInstaller::Approval::~Approval() {}

const WebstoreInstaller::Approval* WebstoreInstaller::GetAssociatedApproval(
    const DownloadItem& download) {
  return static_cast<const Approval*>(download.GetUserData(kApprovalKey));
}

WebstoreInstaller::WebstoreInstaller(Profile* profile,
                                     Delegate* delegate,
                                     NavigationController* controller,
                                     const std::string& id,
                                     scoped_ptr<Approval> approval,
                                     InstallSource source)
    : profile_(profile),
      delegate_(delegate),
      controller_(controller),
      id_(id),
      install_source_(source),
      download_item_(NULL),
      approval_(approval.release()),
      total_modules_(0),
      download_started_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(controller_);

  registrar_.Add(this, chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                 content::Source<CrxInstaller>(NULL));
}

void WebstoreInstaller::Start() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AddRef();  // Balanced in ReportSuccess and ReportFailure.

  if (!Extension::IdIsValid(id_)) {
    ReportFailure(kInvalidIdError, FAILURE_REASON_OTHER);
    return;
  }

  ExtensionService* extension_service =
    ExtensionSystem::Get(profile_)->extension_service();
  if (approval_.get() && approval_->dummy_extension) {
    ExtensionService::ImportStatus status =
      extension_service->CheckImports(approval_->dummy_extension,
                                      &pending_modules_, &pending_modules_);
    // For this case, it is because some imports are not shared modules.
    if (status == ExtensionService::IMPORT_STATUS_UNRECOVERABLE) {
      ReportFailure(kDependencyNotSharedModuleError,
          FAILURE_REASON_DEPENDENCY_NOT_SHARED_MODULE);
      return;
    }
  }

  // Add the extension main module into the list.
  SharedModuleInfo::ImportInfo info;
  info.extension_id = id_;
  pending_modules_.push_back(info);

  total_modules_ = pending_modules_.size();

  // TODO(crbug.com/305343): Query manifest of dependencises before
  // downloading & installing those dependencies.
  DownloadNextPendingModule();

  std::string name;
  if (!approval_->manifest->value()->GetString(manifest_keys::kName, &name)) {
    NOTREACHED();
  }
  extensions::InstallTracker* tracker =
      extensions::InstallTrackerFactory::GetForProfile(profile_);
  tracker->OnBeginExtensionInstall(
      id_, name, approval_->installing_icon, approval_->manifest->is_app(),
      approval_->manifest->is_platform_app());
}

void WebstoreInstaller::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_CRX_INSTALLER_DONE: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      CrxInstaller* installer = content::Source<CrxInstaller>(source).ptr();
      if (extension == NULL && download_item_ != NULL &&
          installer->download_url() == download_item_->GetURL() &&
          installer->profile()->IsSameProfile(profile_)) {
        ReportFailure(kInstallCanceledError, FAILURE_REASON_CANCELLED);
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      CHECK(profile_->IsSameProfile(content::Source<Profile>(source).ptr()));
      const Extension* extension =
          content::Details<const InstalledExtensionInfo>(details)->extension;
      if (pending_modules_.empty())
        return;
      SharedModuleInfo::ImportInfo info = pending_modules_.front();
      if (extension->id() != info.extension_id)
        return;
      pending_modules_.pop_front();

      if (pending_modules_.empty()) {
        CHECK_EQ(extension->id(), id_);
        ReportSuccess();
      } else {
        const Version version_required(info.minimum_version);
        if (version_required.IsValid() &&
            extension->version()->CompareTo(version_required) < 0) {
          // It should not happen, CrxInstaller will make sure the version is
          // equal or newer than version_required.
          ReportFailure(kDependencyNotFoundError,
              FAILURE_REASON_DEPENDENCY_NOT_FOUND);
        } else if (!SharedModuleInfo::IsSharedModule(extension)) {
          // It should not happen, CrxInstaller will make sure it is a shared
          // module.
          ReportFailure(kDependencyNotSharedModuleError,
              FAILURE_REASON_DEPENDENCY_NOT_SHARED_MODULE);
        } else {
          DownloadNextPendingModule();
        }
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR: {
      CrxInstaller* crx_installer = content::Source<CrxInstaller>(source).ptr();
      CHECK(crx_installer);
      if (!profile_->IsSameProfile(crx_installer->profile()))
        return;

      // TODO(rdevlin.cronin): Continue removing std::string errors and
      // replacing with string16. See crbug.com/71980.
      const string16* error = content::Details<const string16>(details).ptr();
      const std::string utf8_error = UTF16ToUTF8(*error);
      if (download_url_ == crx_installer->original_download_url())
        ReportFailure(utf8_error, FAILURE_REASON_OTHER);
      break;
    }

    default:
      NOTREACHED();
  }
}

void WebstoreInstaller::InvalidateDelegate() {
  delegate_ = NULL;
}

void WebstoreInstaller::SetDownloadDirectoryForTests(
    base::FilePath* directory) {
  g_download_directory_for_tests = directory;
}

WebstoreInstaller::~WebstoreInstaller() {
  controller_ = NULL;
  if (download_item_) {
    download_item_->RemoveObserver(this);
    download_item_ = NULL;
  }
}

void WebstoreInstaller::OnDownloadStarted(
    DownloadItem* item, net::Error error) {
  if (!item) {
    DCHECK_NE(net::OK, error);
    ReportFailure(net::ErrorToString(error), FAILURE_REASON_OTHER);
    return;
  }

  DCHECK_EQ(net::OK, error);
  DCHECK(!pending_modules_.empty());
  download_item_ = item;
  download_item_->AddObserver(this);
  if (pending_modules_.size() > 1) {
    // We are downloading a shared module. We need create an approval for it.
    scoped_ptr<Approval> approval = Approval::CreateForSharedModule(profile_);
    const SharedModuleInfo::ImportInfo& info = pending_modules_.front();
    approval->extension_id = info.extension_id;
    const Version version_required(info.minimum_version);

    if (version_required.IsValid()) {
      approval->minimum_version.reset(
          new Version(version_required));
    }
    download_item_->SetUserData(kApprovalKey, approval.release());
  } else {
    // It is for the main module of the extension. We should use the provided
    // |approval_|.
    if (approval_)
      download_item_->SetUserData(kApprovalKey, approval_.release());
  }

  if (!download_started_) {
    if (delegate_)
      delegate_->OnExtensionDownloadStarted(id_, download_item_);
    download_started_ = true;
  }
}

void WebstoreInstaller::OnDownloadUpdated(DownloadItem* download) {
  CHECK_EQ(download_item_, download);

  switch (download->GetState()) {
    case DownloadItem::CANCELLED:
      ReportFailure(kDownloadCanceledError, FAILURE_REASON_CANCELLED);
      break;
    case DownloadItem::INTERRUPTED:
      ReportFailure(kDownloadInterruptedError, FAILURE_REASON_OTHER);
      break;
    case DownloadItem::COMPLETE:
      // Wait for other notifications if the download is really an extension.
      if (!download_crx_util::IsExtensionDownload(*download)) {
        ReportFailure(kInvalidDownloadError, FAILURE_REASON_OTHER);
      } else if (pending_modules_.empty()) {
        // The download is the last module - the extension main module.
        if (delegate_)
          delegate_->OnExtensionDownloadProgress(id_, download);
        extensions::InstallTracker* tracker =
            extensions::InstallTrackerFactory::GetForProfile(profile_);
        tracker->OnDownloadProgress(id_, 100);
      }
      break;
    case DownloadItem::IN_PROGRESS: {
      if (delegate_ && pending_modules_.size() == 1) {
        // Only report download progress for the main module to |delegrate_|.
        delegate_->OnExtensionDownloadProgress(id_, download);
      }
      int percent = download->PercentComplete();
      // Only report progress if precent is more than 0
      if (percent >= 0) {
        int finished_modules = total_modules_ - pending_modules_.size();
        percent = (percent + finished_modules * 100) / total_modules_;
        extensions::InstallTracker* tracker =
          extensions::InstallTrackerFactory::GetForProfile(profile_);
        tracker->OnDownloadProgress(id_, percent);
      }
      break;
    }
    default:
      // Continue listening if the download is not in one of the above states.
      break;
  }
}

void WebstoreInstaller::OnDownloadDestroyed(DownloadItem* download) {
  CHECK_EQ(download_item_, download);
  download_item_->RemoveObserver(this);
  download_item_ = NULL;
}

void WebstoreInstaller::DownloadNextPendingModule() {
  CHECK(!pending_modules_.empty());
  if (pending_modules_.size() == 1) {
    DCHECK_EQ(id_, pending_modules_.front().extension_id);
    DownloadCrx(id_, install_source_);
  } else {
    DownloadCrx(pending_modules_.front().extension_id, INSTALL_SOURCE_OTHER);
  }
}

void WebstoreInstaller::DownloadCrx(
    const std::string& extension_id, InstallSource source) {
  download_url_ = GetWebstoreInstallURL(extension_id, source);
  base::FilePath download_path = DownloadPrefs::FromDownloadManager(
      BrowserContext::GetDownloadManager(profile_))->DownloadPath();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&GetDownloadFilePath, download_path, id_,
        base::Bind(&WebstoreInstaller::StartDownload, this)));
}

// http://crbug.com/165634
// http://crbug.com/126013
// The current working theory is that one of the many pointers dereferenced in
// here is occasionally deleted before all of its referers are nullified,
// probably in a callback race. After this comment is released, the crash
// reports should narrow down exactly which pointer it is.  Collapsing all the
// early-returns into a single branch makes it hard to see exactly which pointer
// it is.
void WebstoreInstaller::StartDownload(const base::FilePath& file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DownloadManager* download_manager =
    BrowserContext::GetDownloadManager(profile_);
  if (file.empty()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!download_manager) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!controller_) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!controller_->GetWebContents()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!controller_->GetWebContents()->GetRenderProcessHost()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!controller_->GetWebContents()->GetRenderViewHost()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!controller_->GetBrowserContext()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!controller_->GetBrowserContext()->GetResourceContext()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }

  // The download url for the given extension is contained in |download_url_|.
  // We will navigate the current tab to this url to start the download. The
  // download system will then pass the crx to the CrxInstaller.
  RecordDownloadSource(DOWNLOAD_INITIATED_BY_WEBSTORE_INSTALLER);
  int render_process_host_id =
    controller_->GetWebContents()->GetRenderProcessHost()->GetID();
  int render_view_host_routing_id =
    controller_->GetWebContents()->GetRenderViewHost()->GetRoutingID();
  content::ResourceContext* resource_context =
    controller_->GetBrowserContext()->GetResourceContext();
  scoped_ptr<DownloadUrlParameters> params(new DownloadUrlParameters(
      download_url_,
      render_process_host_id,
      render_view_host_routing_id ,
      resource_context));
  params->set_file_path(file);
  if (controller_->GetVisibleEntry())
    params->set_referrer(
        content::Referrer(controller_->GetVisibleEntry()->GetURL(),
                          blink::WebReferrerPolicyDefault));
  params->set_callback(base::Bind(&WebstoreInstaller::OnDownloadStarted, this));
  download_manager->DownloadUrl(params.Pass());
}

void WebstoreInstaller::ReportFailure(const std::string& error,
                                      FailureReason reason) {
  if (delegate_) {
    delegate_->OnExtensionInstallFailure(id_, error, reason);
    delegate_ = NULL;
  }

  extensions::InstallTracker* tracker =
      extensions::InstallTrackerFactory::GetForProfile(profile_);
  tracker->OnInstallFailure(id_);

  Release();  // Balanced in Start().
}

void WebstoreInstaller::ReportSuccess() {
  if (delegate_) {
    delegate_->OnExtensionInstallSuccess(id_);
    delegate_ = NULL;
  }

  Release();  // Balanced in Start().
}

}  // namespace extensions
