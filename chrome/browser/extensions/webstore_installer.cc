// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_installer.h"

#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/crx_file/id_util.h"
#include "components/omaha_query_params/omaha_query_params.h"
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
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "net/base/escape.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/file_system_util.h"
#endif

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
const char kDownloadInterruptedError[] = "Download interrupted";
const char kInvalidDownloadError[] =
    "Download was not a valid extension or user script";
const char kDependencyNotFoundError[] = "Dependency not found";
const char kDependencyNotSharedModuleError[] =
    "Dependency is not shared module";
const char kInlineInstallSource[] = "inline";
const char kDefaultInstallSource[] = "ondemand";
const char kAppLauncherInstallSource[] = "applauncher";

// TODO(rockot): Share this duplicated constant with the extension updater.
// See http://crbug.com/371398.
const char kAuthUserQueryKey[] = "authuser";

const size_t kTimeRemainingMinutesThreshold = 1u;

// Folder for downloading crx files from the webstore. This is used so that the
// crx files don't go via the usual downloads folder.
const base::FilePath::CharType kWebstoreDownloadFolder[] =
    FILE_PATH_LITERAL("Webstore Downloads");

base::FilePath* g_download_directory_for_tests = NULL;

// Must be executed on the FILE thread.
void GetDownloadFilePath(
    const base::FilePath& download_directory,
    const std::string& id,
    const base::Callback<void(const base::FilePath&)>& callback) {
  // Ensure the download directory exists. TODO(asargent) - make this use
  // common code from the downloads system.
  if (!base::DirectoryExists(download_directory)) {
    if (!base::CreateDirectory(download_directory)) {
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
      download_directory.AppendASCII(id + "_" + random_number + ".crx");

  int uniquifier =
      base::GetUniquePathNumber(file, base::FilePath::StringType());
  if (uniquifier > 0) {
    file = file.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", uniquifier));
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, file));
}

void MaybeAppendAuthUserParameter(const std::string& authuser, GURL* url) {
  if (authuser.empty())
    return;
  std::string old_query = url->query();
  url::Component query(0, old_query.length());
  url::Component key, value;
  // Ensure that the URL doesn't already specify an authuser parameter.
  while (url::ExtractQueryKeyValue(
             old_query.c_str(), &query, &key, &value)) {
    std::string key_string = old_query.substr(key.begin, key.len);
    if (key_string == kAuthUserQueryKey) {
      return;
    }
  }
  if (!old_query.empty()) {
    old_query += "&";
  }
  std::string authuser_param = base::StringPrintf(
      "%s=%s",
      kAuthUserQueryKey,
      authuser.c_str());

  // TODO(rockot): Share this duplicated code with the extension updater.
  // See http://crbug.com/371398.
  std::string new_query_string = old_query + authuser_param;
  url::Component new_query(0, new_query_string.length());
  url::Replacements<char> replacements;
  replacements.SetQuery(new_query_string.c_str(), new_query);
  *url = url->ReplaceComponents(replacements);
}

}  // namespace

namespace extensions {

// static
GURL WebstoreInstaller::GetWebstoreInstallURL(
    const std::string& extension_id,
    InstallSource source) {
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
  params.push_back("uc");
  std::string url_string = extension_urls::GetWebstoreUpdateUrl().spec();

  GURL url(url_string + "?response=redirect&" +
           omaha_query_params::OmahaQueryParams::Get(
               omaha_query_params::OmahaQueryParams::CRX) +
           "&x=" + net::EscapeQueryParamValue(JoinString(params, '&'), true));
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
      manifest_check_level(MANIFEST_CHECK_LEVEL_STRICT),
      is_ephemeral(false) {
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
  result->skip_post_install_ui = true;
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
                   scoped_ptr<base::DictionaryValue>(
                       parsed_manifest->DeepCopy())));
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
                                     content::WebContents* web_contents,
                                     const std::string& id,
                                     scoped_ptr<Approval> approval,
                                     InstallSource source)
    : content::WebContentsObserver(web_contents),
      extension_registry_observer_(this),
      profile_(profile),
      delegate_(delegate),
      id_(id),
      install_source_(source),
      download_item_(NULL),
      approval_(approval.release()),
      total_modules_(0),
      download_started_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents);

  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                 content::Source<CrxInstaller>(NULL));
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile));
}

void WebstoreInstaller::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AddRef();  // Balanced in ReportSuccess and ReportFailure.

  if (!crx_file::id_util::IdIsValid(id_)) {
    ReportFailure(kInvalidIdError, FAILURE_REASON_OTHER);
    return;
  }

  ExtensionService* extension_service =
    ExtensionSystem::Get(profile_)->extension_service();
  if (approval_.get() && approval_->dummy_extension.get()) {
    extension_service->shared_module_service()->CheckImports(
        approval_->dummy_extension.get(), &pending_modules_, &pending_modules_);
    // Do not check the return value of CheckImports, the CRX installer
    // will report appropriate error messages and fail to install if there
    // is an import error.
  }

  // Add the extension main module into the list.
  SharedModuleInfo::ImportInfo info;
  info.extension_id = id_;
  pending_modules_.push_back(info);

  total_modules_ = pending_modules_.size();

  std::set<std::string> ids;
  std::list<SharedModuleInfo::ImportInfo>::const_iterator i;
  for (i = pending_modules_.begin(); i != pending_modules_.end(); ++i) {
    ids.insert(i->extension_id);
  }
  ExtensionSystem::Get(profile_)->install_verifier()->AddProvisional(ids);

  std::string name;
  if (!approval_->manifest->value()->GetString(manifest_keys::kName, &name)) {
    NOTREACHED();
  }
  extensions::InstallTracker* tracker =
      extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
  extensions::InstallObserver::ExtensionInstallParams params(
      id_,
      name,
      approval_->installing_icon,
      approval_->manifest->is_app(),
      approval_->manifest->is_platform_app());
  params.is_ephemeral = approval_->is_ephemeral;
  tracker->OnBeginExtensionInstall(params);

  tracker->OnBeginExtensionDownload(id_);

  // TODO(crbug.com/305343): Query manifest of dependencies before
  // downloading & installing those dependencies.
  DownloadNextPendingModule();
}

void WebstoreInstaller::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR: {
      CrxInstaller* crx_installer = content::Source<CrxInstaller>(source).ptr();
      CHECK(crx_installer);
      if (crx_installer != crx_installer_.get())
        return;

      // TODO(rdevlin.cronin): Continue removing std::string errors and
      // replacing with base::string16. See crbug.com/71980.
      const base::string16* error =
          content::Details<const base::string16>(details).ptr();
      const std::string utf8_error = base::UTF16ToUTF8(*error);
      crx_installer_ = NULL;
      // ReportFailure releases a reference to this object so it must be the
      // last operation in this method.
      ReportFailure(utf8_error, FAILURE_REASON_OTHER);
      break;
    }

    default:
      NOTREACHED();
  }
}

void WebstoreInstaller::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  CHECK(profile_->IsSameProfile(Profile::FromBrowserContext(browser_context)));
  if (pending_modules_.empty())
    return;
  SharedModuleInfo::ImportInfo info = pending_modules_.front();
  if (extension->id() != info.extension_id)
    return;
  pending_modules_.pop_front();

  // Clean up local state from the current download.
  if (download_item_) {
    download_item_->RemoveObserver(this);
    download_item_->Remove();
    download_item_ = NULL;
  }
  crx_installer_ = NULL;

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
}

void WebstoreInstaller::InvalidateDelegate() {
  delegate_ = NULL;
}

void WebstoreInstaller::SetDownloadDirectoryForTests(
    base::FilePath* directory) {
  g_download_directory_for_tests = directory;
}

WebstoreInstaller::~WebstoreInstaller() {
  if (download_item_) {
    download_item_->RemoveObserver(this);
    download_item_ = NULL;
  }
}

void WebstoreInstaller::OnDownloadStarted(
    DownloadItem* item,
    content::DownloadInterruptReason interrupt_reason) {
  if (!item) {
    DCHECK_NE(content::DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason);
    ReportFailure(content::DownloadInterruptReasonToString(interrupt_reason),
                  FAILURE_REASON_OTHER);
    return;
  }

  DCHECK_EQ(content::DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason);
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
      RecordInterrupt(download);
      ReportFailure(kDownloadInterruptedError, FAILURE_REASON_OTHER);
      break;
    case DownloadItem::COMPLETE:
      // Wait for other notifications if the download is really an extension.
      if (!download_crx_util::IsExtensionDownload(*download)) {
        ReportFailure(kInvalidDownloadError, FAILURE_REASON_OTHER);
      } else {
        if (crx_installer_.get())
          return;  // DownloadItemImpl calls the observer twice, ignore it.
        StartCrxInstaller(*download);

        if (pending_modules_.size() == 1) {
          // The download is the last module - the extension main module.
          if (delegate_)
            delegate_->OnExtensionDownloadProgress(id_, download);
          extensions::InstallTracker* tracker =
              extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
          tracker->OnDownloadProgress(id_, 100);
        }
      }
      // Stop the progress timer if it's running.
      download_progress_timer_.Stop();
      break;
    case DownloadItem::IN_PROGRESS: {
      if (delegate_ && pending_modules_.size() == 1) {
        // Only report download progress for the main module to |delegrate_|.
        delegate_->OnExtensionDownloadProgress(id_, download);
      }
      UpdateDownloadProgress();
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
    const std::string& extension_id,
    InstallSource source) {
  download_url_ = GetWebstoreInstallURL(extension_id, source);
  MaybeAppendAuthUserParameter(approval_->authuser, &download_url_);

  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath download_path = user_data_dir.Append(kWebstoreDownloadFolder);

  base::FilePath download_directory(g_download_directory_for_tests ?
      *g_download_directory_for_tests : download_path);

#if defined(OS_CHROMEOS)
  // Do not use drive for extension downloads.
  if (drive::util::IsUnderDriveMountPoint(download_directory)) {
    download_directory = DownloadPrefs::FromBrowserContext(
        profile_)->GetDefaultDownloadDirectoryForProfile();
  }
#endif

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&GetDownloadFilePath, download_directory, extension_id,
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (file.empty()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }

  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(profile_);
  if (!download_manager) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }

  content::WebContents* contents = web_contents();
  if (!contents) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!contents->GetRenderProcessHost()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!contents->GetRenderViewHost()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }

  content::NavigationController& controller = contents->GetController();
  if (!controller.GetBrowserContext()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }
  if (!controller.GetBrowserContext()->GetResourceContext()) {
    ReportFailure(kDownloadDirectoryError, FAILURE_REASON_OTHER);
    return;
  }

  // The download url for the given extension is contained in |download_url_|.
  // We will navigate the current tab to this url to start the download. The
  // download system will then pass the crx to the CrxInstaller.
  RecordDownloadSource(DOWNLOAD_INITIATED_BY_WEBSTORE_INSTALLER);
  int render_process_host_id = contents->GetRenderProcessHost()->GetID();
  int render_view_host_routing_id =
      contents->GetRenderViewHost()->GetRoutingID();
  content::ResourceContext* resource_context =
      controller.GetBrowserContext()->GetResourceContext();
  scoped_ptr<DownloadUrlParameters> params(new DownloadUrlParameters(
      download_url_,
      render_process_host_id,
      render_view_host_routing_id ,
      resource_context));
  params->set_file_path(file);
  if (controller.GetVisibleEntry())
    params->set_referrer(
        content::Referrer(controller.GetVisibleEntry()->GetURL(),
                          blink::WebReferrerPolicyDefault));
  params->set_callback(base::Bind(&WebstoreInstaller::OnDownloadStarted, this));
  download_manager->DownloadUrl(params.Pass());
}

void WebstoreInstaller::UpdateDownloadProgress() {
  // If the download has gone away, or isn't in progress (in which case we can't
  // give a good progress estimate), stop any running timers and return.
  if (!download_item_ ||
      download_item_->GetState() != DownloadItem::IN_PROGRESS) {
    download_progress_timer_.Stop();
    return;
  }

  int percent = download_item_->PercentComplete();
  // Only report progress if percent is more than 0 or we have finished
  // downloading at least one of the pending modules.
  int finished_modules = total_modules_ - pending_modules_.size();
  if (finished_modules > 0 && percent < 0)
    percent = 0;
  if (percent >= 0) {
    percent = (percent + (finished_modules * 100)) / total_modules_;
    extensions::InstallTracker* tracker =
        extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
    tracker->OnDownloadProgress(id_, percent);
  }

  // If there's enough time remaining on the download to warrant an update,
  // set the timer (overwriting any current timers). Otherwise, stop the
  // timer.
  base::TimeDelta time_remaining;
  if (download_item_->TimeRemaining(&time_remaining) &&
      time_remaining >
          base::TimeDelta::FromSeconds(kTimeRemainingMinutesThreshold)) {
    download_progress_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kTimeRemainingMinutesThreshold),
        this,
        &WebstoreInstaller::UpdateDownloadProgress);
  } else {
    download_progress_timer_.Stop();
  }
}

void WebstoreInstaller::StartCrxInstaller(const DownloadItem& download) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!crx_installer_.get());

  ExtensionService* service = ExtensionSystem::Get(profile_)->
      extension_service();
  CHECK(service);

  const Approval* approval = GetAssociatedApproval(download);
  DCHECK(approval);

  crx_installer_ = download_crx_util::CreateCrxInstaller(profile_, download);

  crx_installer_->set_expected_id(approval->extension_id);
  crx_installer_->set_is_gallery_install(true);
  crx_installer_->set_allow_silent_install(true);

  crx_installer_->InstallCrx(download.GetFullPath());
}

void WebstoreInstaller::ReportFailure(const std::string& error,
                                      FailureReason reason) {
  if (delegate_) {
    delegate_->OnExtensionInstallFailure(id_, error, reason);
    delegate_ = NULL;
  }

  extensions::InstallTracker* tracker =
      extensions::InstallTrackerFactory::GetForBrowserContext(profile_);
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

void WebstoreInstaller::RecordInterrupt(const DownloadItem* download) const {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Extensions.WebstoreDownload.InterruptReason",
                              download->GetLastReason());

  // Use logarithmic bin sizes up to 1 TB.
  const int kNumBuckets = 30;
  const int64 kMaxSizeKb = 1 << kNumBuckets;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Extensions.WebstoreDownload.InterruptReceivedKBytes",
      download->GetReceivedBytes() / 1024,
      1,
      kMaxSizeKb,
      kNumBuckets);
  int64 total_bytes = download->GetTotalBytes();
  if (total_bytes >= 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Extensions.WebstoreDownload.InterruptTotalKBytes",
        total_bytes / 1024,
        1,
        kMaxSizeKb,
        kNumBuckets);
  }
  UMA_HISTOGRAM_BOOLEAN(
      "Extensions.WebstoreDownload.InterruptTotalSizeUnknown",
      total_bytes <= 0);
}

}  // namespace extensions
