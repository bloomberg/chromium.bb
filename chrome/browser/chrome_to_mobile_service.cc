// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_to_mobile_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_helpers.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/notifier/invalidation_util.h"

namespace {

// The default enabled/disabled state of the Chrome To Mobile feature.
const bool kChromeToMobileEnabled = true;

// The maximum number of retries for the URLFetcher requests.
const size_t kMaxRetries = 1;

// The number of hours to delay before retrying authentication on failure.
const size_t kAuthRetryDelayHours = 6;

// The number of hours before subsequent search requests are allowed.
// This value is used to throttle expensive cloud print search requests.
// Note that this limitation does not hold across application restarts.
const int kSearchRequestDelayHours = 24;

// The sync invalidation object ID for Chrome to Mobile's mobile device list.
// This corresponds with cloud print's server-side invalidation object ID.
// Meaning: "U" == "User", "CM" == "Chrome to Mobile", "MLST" == "Mobile LiST".
const char kSyncInvalidationObjectIdChromeToMobileDeviceList[] = "UCMMLST";

// The cloud print OAuth2 scope and 'printer' type of compatible mobile devices.
const char kCloudPrintAuth[] = "https://www.googleapis.com/auth/cloudprint";
const char kTypeAndroid[] = "ANDROID_CHROME_SNAPSHOT";
const char kTypeIOS[] = "IOS_CHROME_SNAPSHOT";

// The Chrome To Mobile requestor type; used by services for filtering.
const char kChromeToMobileRequestor[] = "requestor=chrome-to-mobile";

// Get the job type string for a cloud print job submission.
std::string GetType(const ChromeToMobileService::JobData& data) {
  if (data.type == ChromeToMobileService::URL)
    return "url";
  if (data.type == ChromeToMobileService::DELAYED_SNAPSHOT)
    return "url_with_delayed_snapshot";
  DCHECK_EQ(data.type, ChromeToMobileService::SNAPSHOT);
  return "snapshot";
}

// Get the JSON string for cloud print job submissions.
std::string GetJSON(const ChromeToMobileService::JobData& data) {
  DictionaryValue json;
  switch (data.mobile_os) {
    case ChromeToMobileService::ANDROID:
      json.SetString("url", data.url.spec());
      json.SetString("type", GetType(data));
      if (data.type != ChromeToMobileService::URL)
        json.SetString("snapID", data.snapshot_id);
      break;
    case ChromeToMobileService::IOS:
      // TODO(chenyu|msw): Currently only sends an alert; include the url here?
      json.SetString("aps.alert.body", "A print job is available");
      json.SetString("aps.alert.loc-key", "IDS_CHROME_TO_DEVICE_SNAPSHOTS_IOS");
      break;
    default:
      NOTREACHED() << "Unknown mobile_os " << data.mobile_os;
      break;
  }
  std::string json_string;
  base::JSONWriter::Write(&json, &json_string);
  return json_string;
}

// Get the POST content type for a cloud print job submission.
std::string GetContentType(ChromeToMobileService::JobType type) {
  return (type == ChromeToMobileService::SNAPSHOT) ?
      "multipart/related" : "text/plain";
}

// Utility function to call cloud_print::AddMultipartValueForUpload.
void AddValue(const std::string& value_name,
              const std::string& value,
              const std::string& mime_boundary,
              std::string* post_data) {
  cloud_print::AddMultipartValueForUpload(value_name, value, mime_boundary,
                                          std::string(), post_data);
}

// Get the URL for cloud print device search; appends a requestor query param.
GURL GetSearchURL(const GURL& cloud_print_url) {
  GURL search_url = cloud_print::GetUrlForSearch(cloud_print_url);
  GURL::Replacements replacements;
  std::string query(kChromeToMobileRequestor);
  replacements.SetQueryStr(query);
  return search_url.ReplaceComponents(replacements);
}

// A callback to continue snapshot generation after creating the temp file.
typedef base::Callback<void(const FilePath& path, bool success)>
    CreateSnapshotFileCallback;

// Create a temp file and post the callback on the UI thread with the results.
// Call this as a BlockingPoolTask to avoid the FILE thread.
void CreateSnapshotFile(CreateSnapshotFileCallback callback) {
  FilePath file;
  bool success = file_util::CreateTemporaryFile(&file);
  if (!content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                        base::Bind(callback, file, success))) {
    NOTREACHED();
  }
}

// Delete the snapshot file; DCHECK, but really ignore the result of the delete.
// Call this as a BlockingPoolSequencedTask [after posting SubmitSnapshotFile].
void DeleteSnapshotFile(const FilePath& snapshot) {
  bool success = file_util::Delete(snapshot, false);
  DCHECK(success);
}

}  // namespace

ChromeToMobileService::Observer::~Observer() {}

ChromeToMobileService::JobData::JobData() : mobile_os(ANDROID), type(URL) {}

ChromeToMobileService::JobData::~JobData() {}

// static
bool ChromeToMobileService::IsChromeToMobileEnabled() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kDisableChromeToMobile))
    return false;

  if (command_line->HasSwitch(switches::kEnableChromeToMobile))
    return true;

  return kChromeToMobileEnabled;
}

// static
void ChromeToMobileService::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterListPref(prefs::kChromeToMobileDeviceList,
                          PrefService::UNSYNCABLE_PREF);
}

ChromeToMobileService::ChromeToMobileService(Profile* profile)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      profile_(profile),
      sync_invalidation_enabled_(false) {
  // TODO(msw): Unit tests do not provide profiles; see http://crbug.com/122183
  ProfileSyncService* profile_sync_service =
      profile_ ? ProfileSyncServiceFactory::GetForProfile(profile_) : NULL;
  if (profile_sync_service) {
    CloudPrintURL cloud_print_url(profile_);
    cloud_print_url_ = cloud_print_url.GetCloudPrintServiceURL();
    // Register for cloud print device list invalidation notifications.
    // TODO(msw|akalin): Initialize |sync_invalidation_enabled_| properly.
    profile_sync_service->RegisterInvalidationHandler(this);
    syncer::ObjectIdSet ids;
    ids.insert(invalidation::ObjectId(
        ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
        kSyncInvalidationObjectIdChromeToMobileDeviceList));
    profile_sync_service->UpdateRegisteredInvalidationIds(this, ids);
  }
}

ChromeToMobileService::~ChromeToMobileService() {
  while (!snapshots_.empty())
    DeleteSnapshot(*snapshots_.begin());
}

bool ChromeToMobileService::HasMobiles() const {
  const base::ListValue* mobiles = GetMobiles();
  return mobiles && !mobiles->empty();
}

const base::ListValue* ChromeToMobileService::GetMobiles() const {
  return sync_invalidation_enabled_ ?
      profile_->GetPrefs()->GetList(prefs::kChromeToMobileDeviceList) : NULL;
}

void ChromeToMobileService::GenerateSnapshot(Browser* browser,
                                             base::WeakPtr<Observer> observer) {
  // Callback SnapshotFileCreated from CreateSnapshotFile to continue.
  CreateSnapshotFileCallback callback =
      base::Bind(&ChromeToMobileService::SnapshotFileCreated,
                 weak_ptr_factory_.GetWeakPtr(), observer,
                 browser->session_id().id());
  // Create a temporary file via the blocking pool for snapshot storage.
  if (!content::BrowserThread::PostBlockingPoolTask(FROM_HERE,
          base::Bind(&CreateSnapshotFile, callback))) {
    NOTREACHED();
  }
}

void ChromeToMobileService::SendToMobile(const base::DictionaryValue* mobile,
                                         const FilePath& snapshot,
                                         Browser* browser,
                                         base::WeakPtr<Observer> observer) {
  if (access_token_.empty()) {
    // Enqueue this task to perform after obtaining an access token.
    task_queue_.push(base::Bind(&ChromeToMobileService::SendToMobile,
        weak_ptr_factory_.GetWeakPtr(), base::Owned(mobile->DeepCopy()),
        snapshot, browser, observer));
    RequestAccessToken();
    return;
  }

  LogMetric(SENDING_URL);

  JobData data;
  std::string mobile_os;
  if (!mobile->GetString("type", &mobile_os))
    NOTREACHED();
  data.mobile_os = (mobile_os.compare(kTypeAndroid) == 0) ?
      ChromeToMobileService::ANDROID : ChromeToMobileService::IOS;
  if (!mobile->GetString("id", &data.mobile_id))
    NOTREACHED();
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser);
  data.url = web_contents->GetURL();
  data.title = web_contents->GetTitle();
  data.snapshot = snapshot;
  data.snapshot_id = base::GenerateGUID();
  data.type = !snapshot.empty() ? DELAYED_SNAPSHOT : URL;

  net::URLFetcher* submit_url = CreateRequest(data);
  request_observer_map_[submit_url] = observer;
  SendRequest(submit_url, data);

  if (data.type == DELAYED_SNAPSHOT) {
    LogMetric(SENDING_SNAPSHOT);

    data.type = SNAPSHOT;
    net::URLFetcher* submit_snapshot = CreateRequest(data);
    request_observer_map_[submit_snapshot] = observer;
    if (!content::BrowserThread::PostBlockingPoolSequencedTask(
            data.snapshot.AsUTF8Unsafe(), FROM_HERE,
            base::Bind(&ChromeToMobileService::SendRequest,
                       weak_ptr_factory_.GetWeakPtr(),
                       submit_snapshot, data))) {
      NOTREACHED();
    }
  }
}

void ChromeToMobileService::DeleteSnapshot(const FilePath& snapshot) {
  DCHECK(snapshot.empty() || snapshots_.find(snapshot) != snapshots_.end());
  if (snapshots_.find(snapshot) != snapshots_.end()) {
    if (!snapshot.empty()) {
      if (!content::BrowserThread::PostBlockingPoolSequencedTask(
              snapshot.AsUTF8Unsafe(), FROM_HERE,
              base::Bind(&DeleteSnapshotFile, snapshot))) {
        NOTREACHED();
      }
    }
    snapshots_.erase(snapshot);
  }
}

void ChromeToMobileService::LogMetric(Metric metric) const {
  UMA_HISTOGRAM_ENUMERATION("ChromeToMobile.Service", metric, NUM_METRICS);
}

void ChromeToMobileService::LearnMore(Browser* browser) const {
  LogMetric(LEARN_MORE_CLICKED);
  chrome::NavigateParams params(browser,
      GURL(chrome::kChromeToMobileLearnMoreURL), content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

void ChromeToMobileService::Shutdown() {
  // TODO(msw): Unit tests do not provide profiles; see http://crbug.com/122183
  // Unregister for cloud print device list invalidation notifications.
  ProfileSyncService* profile_sync_service =
      profile_ ? ProfileSyncServiceFactory::GetForProfile(profile_) : NULL;
  if (profile_sync_service)
    profile_sync_service->UnregisterInvalidationHandler(this);
}

void ChromeToMobileService::OnURLFetchComplete(const net::URLFetcher* source) {
  if (source->GetURL() == GetSearchURL(cloud_print_url_))
    HandleSearchResponse(source);
  else
    HandleSubmitResponse(source);
}

void ChromeToMobileService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TOKEN_AVAILABLE);
  TokenService::TokenAvailableDetails* token_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();
  // Invalidate the cloud print access token on Gaia login token updates.
  if (token_details->service() == GaiaConstants::kGaiaOAuth2LoginRefreshToken)
    access_token_.clear();
}

void ChromeToMobileService::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK(!access_token.empty());
  access_token_fetcher_.reset();
  auth_retry_timer_.Stop();
  access_token_ = access_token;

  while (!task_queue_.empty()) {
    // Post all tasks that were queued and waiting on a valid access token.
    if (!content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                          task_queue_.front())) {
      NOTREACHED();
    }
    task_queue_.pop();
  }
}

void ChromeToMobileService::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  access_token_fetcher_.reset();
  auth_retry_timer_.Stop();

  auth_retry_timer_.Start(
      FROM_HERE, base::TimeDelta::FromHours(kAuthRetryDelayHours),
      this, &ChromeToMobileService::RequestAccessToken);
}

void ChromeToMobileService::OnNotificationsEnabled() {
  sync_invalidation_enabled_ = true;
  UpdateCommandState();
}

void ChromeToMobileService::OnNotificationsDisabled(
    syncer::NotificationsDisabledReason reason) {
  sync_invalidation_enabled_ = false;
  UpdateCommandState();
}

void ChromeToMobileService::OnIncomingNotification(
    const syncer::ObjectIdPayloadMap& id_payloads,
    syncer::IncomingNotificationSource source) {
  DCHECK_EQ(id_payloads.size(), 1U);
  DCHECK_EQ(id_payloads.count(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      kSyncInvalidationObjectIdChromeToMobileDeviceList)), 1U);
  RequestDeviceSearch();
}

const std::string& ChromeToMobileService::GetAccessTokenForTest() const {
  return access_token_;
}

void ChromeToMobileService::SetAccessTokenForTest(
    const std::string& access_token) {
  access_token_ = access_token;
}

void ChromeToMobileService::UpdateCommandState() const {
  // Ensure the feature is not disabled by commandline options.
  DCHECK(IsChromeToMobileEnabled());
  const bool has_mobiles = HasMobiles();
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end(); ++i) {
    Browser* browser = *i;
    if (browser->profile() == profile_)
      browser->command_controller()->SendToMobileStateChanged(has_mobiles);
  }
}

void ChromeToMobileService::SnapshotFileCreated(
    base::WeakPtr<Observer> observer,
    SessionID::id_type browser_id,
    const FilePath& path,
    bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Track the set of temporary files to be deleted later.
  snapshots_.insert(path);

  Browser* browser = browser::FindBrowserWithID(browser_id);
  if (success && browser && chrome::GetActiveWebContents(browser)) {
    // Generate the snapshot and have the observer be called back on completion.
    chrome::GetActiveWebContents(browser)->GenerateMHTML(path,
        base::Bind(&Observer::SnapshotGenerated, observer));
  } else if (observer.get()) {
    // Signal snapshot generation failure.
    observer->SnapshotGenerated(FilePath(), 0);
  }
}

net::URLFetcher* ChromeToMobileService::CreateRequest(const JobData& data) {
  net::URLFetcher* request = net::URLFetcher::Create(
      cloud_print::GetUrlForSubmit(cloud_print_url_),
      net::URLFetcher::POST, this);
  InitRequest(request);
  return request;
}

void ChromeToMobileService::InitRequest(net::URLFetcher* request) {
  request->SetRequestContext(profile_->GetRequestContext());
  request->SetMaxRetries(kMaxRetries);
  DCHECK(!access_token_.empty());
  request->SetExtraRequestHeaders("Authorization: OAuth " +
      access_token_ + "\r\n" + cloud_print::kChromeCloudPrintProxyHeader);
  request->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
}

void ChromeToMobileService::SendRequest(net::URLFetcher* request,
                                        const JobData& data) {
  std::string post, bound;
  cloud_print::CreateMimeBoundaryForUpload(&bound);
  AddValue("printerid", UTF16ToUTF8(data.mobile_id), bound, &post);
  switch (data.mobile_os) {
    case ChromeToMobileService::ANDROID:
      AddValue("tag", "__c2dm__job_data=" + GetJSON(data), bound, &post);
      break;
    case ChromeToMobileService::IOS:
      AddValue("tag", "__snapshot_id=" + data.snapshot_id, bound, &post);
      AddValue("tag", "__snapshot_type=" + GetType(data), bound, &post);
      if (data.type == ChromeToMobileService::SNAPSHOT) {
        AddValue("tag", "__apns__suppress_notification", bound, &post);
      } else {
        const std::string url = data.url.spec();
        AddValue("tag", "__apns__payload=" + GetJSON(data), bound, &post);
        AddValue("tag", "__apns__original_url=" + url, bound, &post);
      }
      break;
    default:
      NOTREACHED() << "Unknown mobile_os " << data.mobile_os;
      break;
  }

  AddValue("title", UTF16ToUTF8(data.title), bound, &post);
  AddValue("contentType", GetContentType(data.type), bound, &post);

  // Add the snapshot or use dummy content to workaround a URL submission error.
  std::string file = "dummy";
  if (data.type == ChromeToMobileService::SNAPSHOT &&
      (!file_util::ReadFileToString(data.snapshot, &file) || file.empty())) {
    LogMetric(ChromeToMobileService::SNAPSHOT_ERROR);
    return;
  }
  cloud_print::AddMultipartValueForUpload("content", file, bound,
                                          "text/mhtml", &post);

  post.append("--" + bound + "--\r\n");
  request->SetUploadData("multipart/form-data; boundary=" + bound, post);
  request->Start();
}

void ChromeToMobileService::RequestAccessToken() {
  // Register to observe Gaia login refresh token updates.
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (registrar_.IsEmpty())
    registrar_.Add(this, chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   content::Source<TokenService>(token_service));

  // Deny concurrent requests and bail without a valid Gaia login refresh token.
  if (access_token_fetcher_.get() || !token_service->HasOAuthLoginToken())
    return;

  auth_retry_timer_.Stop();
  access_token_fetcher_.reset(
      new OAuth2AccessTokenFetcher(this, profile_->GetRequestContext()));
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  access_token_fetcher_->Start(gaia_urls->oauth2_chrome_client_id(),
                               gaia_urls->oauth2_chrome_client_secret(),
                               token_service->GetOAuth2LoginRefreshToken(),
                               std::vector<std::string>(1, kCloudPrintAuth));
}

void ChromeToMobileService::RequestDeviceSearch() {
  DCHECK(sync_invalidation_enabled_);
  if (access_token_.empty()) {
    // Enqueue this task to perform after obtaining an access token.
    task_queue_.push(base::Bind(&ChromeToMobileService::RequestDeviceSearch,
                                weak_ptr_factory_.GetWeakPtr()));
    RequestAccessToken();
    return;
  }

  LogMetric(DEVICES_REQUESTED);

  net::URLFetcher* search_request = net::URLFetcher::Create(
      GetSearchURL(cloud_print_url_), net::URLFetcher::GET, this);
  InitRequest(search_request);
  search_request->Start();
}

void ChromeToMobileService::HandleSearchResponse(
    const net::URLFetcher* source) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(source->GetURL(), GetSearchURL(cloud_print_url_));

  std::string data;
  ListValue* list = NULL;
  DictionaryValue* dictionary = NULL;
  source->GetResponseAsString(&data);
  scoped_ptr<Value> json(base::JSONReader::Read(data));
  if (json.get() && json->GetAsDictionary(&dictionary) && dictionary &&
      dictionary->GetList(cloud_print::kPrinterListValue, &list)) {
    ListValue mobiles;
    std::string type, name, id;
    DictionaryValue* printer = NULL;
    DictionaryValue* mobile = NULL;
    for (size_t index = 0; index < list->GetSize(); ++index) {
      if (list->GetDictionary(index, &printer) &&
          printer->GetString("type", &type) &&
          (type.compare(kTypeAndroid) == 0 || type.compare(kTypeIOS) == 0)) {
        // Copy just the requisite values from the full |printer| definition.
        if (printer->GetString("displayName", &name) &&
            printer->GetString("id", &id)) {
          mobile = new DictionaryValue();
          mobile->SetString("type", type);
          mobile->SetString("name", name);
          mobile->SetString("id", id);
          mobiles.Append(mobile);
        } else {
          NOTREACHED();
        }
      }
    }

    // Update the cached mobile device list in profile prefs.
    profile_->GetPrefs()->Set(prefs::kChromeToMobileDeviceList, mobiles);

    if (HasMobiles())
      LogMetric(DEVICES_AVAILABLE);
    UpdateCommandState();
  }
}

void ChromeToMobileService::HandleSubmitResponse(
    const net::URLFetcher* source) {
  // Get the observer for this response; bail if there is none or it is NULL.
  RequestObserverMap::iterator i = request_observer_map_.find(source);
  if (i == request_observer_map_.end())
    return;
  base::WeakPtr<Observer> observer = i->second;
  request_observer_map_.erase(i);
  if (!observer.get())
    return;

  // Get the success value from the CloudPrint server response data.
  std::string data;
  source->GetResponseAsString(&data);
  bool success = false;
  DictionaryValue* dictionary = NULL;
  scoped_ptr<Value> json(base::JSONReader::Read(data));
  if (json.get() && json->GetAsDictionary(&dictionary) && dictionary)
    dictionary->GetBoolean("success", &success);

  // Check if the observer is waiting on a second response (url and snapshot).
  RequestObserverMap::iterator other = request_observer_map_.begin();
  for (; other != request_observer_map_.end(); ++other) {
    if (other->second == observer) {
      // Do not call OnSendComplete for observers waiting on a second response.
      if (success)
        return;

      // Ensure a second response is not sent after reporting failure below.
      request_observer_map_.erase(other);
      break;
    }
  }

  LogMetric(success ? SEND_SUCCESS : SEND_ERROR);
  observer->OnSendComplete(success);
}
