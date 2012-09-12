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
#include "chrome/common/extensions/extension_switch_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/notifier/invalidation_util.h"

namespace {

// The maximum number of retries for the URLFetcher requests.
const size_t kMaxRetries = 5;

// The number of hours to delay before retrying certain failed operations.
const size_t kDelayHours = 1;

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

// Log a metric for the "ChromeToMobile.Service" histogram.
void LogServiceMetric(ChromeToMobileService::Metric metric) {
  UMA_HISTOGRAM_ENUMERATION("ChromeToMobile.Service", metric,
                            ChromeToMobileService::NUM_METRICS);
}

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
typedef base::Callback<void(const FilePath& path)> CreateSnapshotFileCallback;

// Create a temp file and post the callback on the UI thread with the results.
// Call this as a BlockingPoolTask to avoid the FILE thread.
void CreateSnapshotFile(CreateSnapshotFileCallback callback) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  FilePath file;
  if (!file_util::CreateTemporaryFile(&file))
    file.clear();
  if (!content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                        base::Bind(callback, file))) {
    LogServiceMetric(ChromeToMobileService::SNAPSHOT_ERROR);
    NOTREACHED();
  }
}

// A callback to continue sending the snapshot data after reading the file.
typedef base::Callback<void(scoped_ptr<ChromeToMobileService::JobData> data)>
    ReadSnapshotFileCallback;

// Read the temp file and post the callback on the UI thread with the results.
// Call this as a BlockingPoolTask to avoid the FILE thread.
void ReadSnapshotFile(scoped_ptr<ChromeToMobileService::JobData> data,
                      ReadSnapshotFileCallback callback) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!file_util::ReadFileToString(data->snapshot, &data->snapshot_content))
    data->snapshot_content.clear();
  if (!content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
          base::Bind(callback, base::Passed(&data)))) {
    LogServiceMetric(ChromeToMobileService::SNAPSHOT_ERROR);
    NOTREACHED();
  }
}

// Delete the snapshot file; DCHECK, but really ignore the result of the delete.
// Call this as a BlockingPoolSequencedTask [after posting SubmitSnapshotFile].
void DeleteSnapshotFile(const FilePath& snapshot) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  bool success = file_util::Delete(snapshot, false);
  DCHECK(success);
}

}  // namespace

ChromeToMobileService::Observer::~Observer() {}

ChromeToMobileService::JobData::JobData() : mobile_os(ANDROID), type(URL) {}

ChromeToMobileService::JobData::~JobData() {}

// static
bool ChromeToMobileService::IsChromeToMobileEnabled() {
  // Chrome To Mobile is currently gated on the Action Box UI.
  return extensions::switch_utils::IsActionBoxEnabled();
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
    sync_invalidation_enabled_ =
        (profile_sync_service->GetInvalidatorState() ==
         syncer::INVALIDATIONS_ENABLED);
    // Register for cloud print device list invalidation notifications.
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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Callback SnapshotFileCreated from CreateSnapshotFile to continue.
  CreateSnapshotFileCallback callback =
      base::Bind(&ChromeToMobileService::SnapshotFileCreated,
                 weak_ptr_factory_.GetWeakPtr(), observer,
                 browser->session_id().id());
  // Create a temporary file via the blocking pool for snapshot storage.
  if (!content::BrowserThread::PostBlockingPoolTask(FROM_HERE,
          base::Bind(&CreateSnapshotFile, callback))) {
    LogMetric(SNAPSHOT_ERROR);
    NOTREACHED();
  }
}

void ChromeToMobileService::SendToMobile(const base::DictionaryValue* mobile,
                                         const FilePath& snapshot,
                                         Browser* browser,
                                         base::WeakPtr<Observer> observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (access_token_.empty()) {
    // Enqueue this task to perform after obtaining an access token.
    task_queue_.push(base::Bind(&ChromeToMobileService::SendToMobile,
        weak_ptr_factory_.GetWeakPtr(), base::Owned(mobile->DeepCopy()),
        snapshot, browser, observer));
    RequestAccessToken();
    return;
  }

  scoped_ptr<JobData> data(new JobData());
  std::string mobile_os;
  if (!mobile->GetString("type", &mobile_os))
    NOTREACHED();
  data->mobile_os = (mobile_os.compare(kTypeAndroid) == 0) ? ANDROID : IOS;
  if (!mobile->GetString("id", &data->mobile_id))
    NOTREACHED();
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser);
  data->url = web_contents->GetURL();
  data->title = web_contents->GetTitle();
  data->snapshot = snapshot;
  data->snapshot_id = base::GenerateGUID();
  data->type = !snapshot.empty() ? DELAYED_SNAPSHOT : URL;
  SendJobRequest(observer, *data);

  if (data->type == DELAYED_SNAPSHOT) {
    // Callback SnapshotFileRead from ReadSnapshotFile to continue.
    ReadSnapshotFileCallback callback =
        base::Bind(&ChromeToMobileService::SnapshotFileRead,
                   weak_ptr_factory_.GetWeakPtr(), observer);
    std::string sequence_token_name = data->snapshot.AsUTF8Unsafe();
    if (!content::BrowserThread::PostBlockingPoolSequencedTask(
            sequence_token_name, FROM_HERE,
            base::Bind(&ReadSnapshotFile, base::Passed(&data), callback))) {
      LogMetric(SNAPSHOT_ERROR);
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
        LogMetric(SNAPSHOT_ERROR);
        NOTREACHED();
      }
    }
    snapshots_.erase(snapshot);
  }
}

void ChromeToMobileService::LogMetric(Metric metric) const {
  LogServiceMetric(metric);
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

  // Remove the URLFetcher from the ScopedVector; this deletes the URLFetcher.
  for (ScopedVector<net::URLFetcher>::iterator it = url_fetchers_.begin();
       it != url_fetchers_.end(); ++it) {
    if (*it == source) {
      url_fetchers_.erase(it);
      break;
    }
  }
}

void ChromeToMobileService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TOKEN_AVAILABLE);
  TokenService::TokenAvailableDetails* token_details =
      content::Details<TokenService::TokenAvailableDetails>(details).ptr();
  // Invalidate the cloud print access token on Gaia login token updates.
  if (token_details->service() == GaiaConstants::kGaiaOAuth2LoginRefreshToken ||
      token_details->service() == GaiaConstants::kGaiaOAuth2LoginAccessToken)
    access_token_.clear();
}

void ChromeToMobileService::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK(!access_token.empty());
  access_token_fetcher_.reset();
  auth_retry_timer_.Stop();
  access_token_ = access_token;

  // Post a delayed task to invalidate the access token at its expiration time.
  if (!content::BrowserThread::PostDelayedTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&ChromeToMobileService::ClearAccessToken,
                     weak_ptr_factory_.GetWeakPtr()),
          expiration_time - base::Time::Now())) {
    NOTREACHED();
  }

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
  LogMetric(BAD_TOKEN);
  access_token_.clear();
  access_token_fetcher_.reset();
  auth_retry_timer_.Stop();

  base::TimeDelta delay = std::max(base::TimeDelta::FromHours(kDelayHours),
                                   auth_retry_timer_.GetCurrentDelay() * 2);
  auth_retry_timer_.Start(FROM_HERE, delay, this,
                          &ChromeToMobileService::RequestAccessToken);

  // Clear the mobile list, which may be (or become) out of date.
  ListValue empty;
  profile_->GetPrefs()->Set(prefs::kChromeToMobileDeviceList, empty);
  UpdateCommandState();
}

void ChromeToMobileService::OnInvalidatorStateChange(
    syncer::InvalidatorState state) {
  sync_invalidation_enabled_ = (state == syncer::INVALIDATIONS_ENABLED);
  UpdateCommandState();
}

void ChromeToMobileService::OnIncomingInvalidation(
    const syncer::ObjectIdStateMap& id_state_map,
    syncer::IncomingInvalidationSource source) {
  DCHECK_EQ(1U, id_state_map.size());
  DCHECK_EQ(1U, id_state_map.count(invalidation::ObjectId(
      ipc::invalidation::ObjectSource::CHROME_COMPONENTS,
      kSyncInvalidationObjectIdChromeToMobileDeviceList)));
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
    const FilePath& path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Track the set of temporary files to be deleted later.
  snapshots_.insert(path);

  Browser* browser = browser::FindBrowserWithID(browser_id);
  if (!path.empty() && browser && chrome::GetActiveWebContents(browser)) {
    // Generate the snapshot and have the observer be called back on completion.
    chrome::GetActiveWebContents(browser)->GenerateMHTML(path,
        base::Bind(&Observer::SnapshotGenerated, observer));
  } else {
    LogMetric(SNAPSHOT_ERROR);
    // Signal snapshot generation failure.
    if (observer.get())
      observer->SnapshotGenerated(FilePath(), 0);
  }
}

void ChromeToMobileService::SnapshotFileRead(base::WeakPtr<Observer> observer,
                                             scoped_ptr<JobData> data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (data->snapshot_content.empty()) {
    LogMetric(SNAPSHOT_ERROR);
    return;
  }

  data->type = SNAPSHOT;
  SendJobRequest(observer, *data);
}

void ChromeToMobileService::InitRequest(net::URLFetcher* request) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  request->SetRequestContext(profile_->GetRequestContext());
  request->SetMaxRetries(kMaxRetries);
  DCHECK(!access_token_.empty());
  request->SetExtraRequestHeaders("Authorization: OAuth " +
      access_token_ + "\r\n" + cloud_print::kChromeCloudPrintProxyHeader);
  request->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
}

void ChromeToMobileService::SendJobRequest(base::WeakPtr<Observer> observer,
                                           const JobData& data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string post, bound;
  cloud_print::CreateMimeBoundaryForUpload(&bound);
  AddValue("printerid", UTF16ToUTF8(data.mobile_id), bound, &post);
  switch (data.mobile_os) {
    case ANDROID:
      AddValue("tag", "__c2dm__job_data=" + GetJSON(data), bound, &post);
      break;
    case IOS:
      AddValue("tag", "__snapshot_id=" + data.snapshot_id, bound, &post);
      AddValue("tag", "__snapshot_type=" + GetType(data), bound, &post);
      if (data.type == SNAPSHOT) {
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
  cloud_print::AddMultipartValueForUpload("content",
      data.snapshot_content.empty() ? "content" : data.snapshot_content,
      bound, "text/mhtml", &post);
  post.append("--" + bound + "--\r\n");

  LogMetric(data.type == SNAPSHOT ? SENDING_SNAPSHOT : SENDING_URL);
  net::URLFetcher* request =
      net::URLFetcher::Create(cloud_print::GetUrlForSubmit(cloud_print_url_),
                              net::URLFetcher::POST, this);
  url_fetchers_.push_back(request);
  InitRequest(request);
  request_observer_map_[request] = observer;
  request->SetUploadData("multipart/form-data; boundary=" + bound, post);
  request->Start();
}

void ChromeToMobileService::ClearAccessToken() {
  access_token_.clear();
}

void ChromeToMobileService::RequestAccessToken() {
  // Register to observe Gaia login refresh token updates.
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  if (registrar_.IsEmpty())
    registrar_.Add(this, chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   content::Source<TokenService>(token_service));

  // Deny concurrent requests.
  if (access_token_fetcher_.get())
    return;

  // Handle invalid login refresh tokens as a failure.
  if (!token_service->HasOAuthLoginToken()) {
    OnGetTokenFailure(GoogleServiceAuthError(GoogleServiceAuthError::NONE));
    return;
  }

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
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  search_retry_timer_.Stop();
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
  url_fetchers_.push_back(search_request);
  InitRequest(search_request);
  search_request->Start();
}

void ChromeToMobileService::HandleSearchResponse(
    const net::URLFetcher* source) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK_EQ(source->GetURL(), GetSearchURL(cloud_print_url_));

  ListValue mobiles;
  std::string data;
  ListValue* list = NULL;
  DictionaryValue* dictionary = NULL;
  source->GetResponseAsString(&data);
  scoped_ptr<Value> json(base::JSONReader::Read(data));
  if (json.get() && json->GetAsDictionary(&dictionary) && dictionary &&
      dictionary->GetList(cloud_print::kPrinterListValue, &list)) {
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
  } else if (source->GetResponseCode() == net::HTTP_FORBIDDEN) {
    LogMetric(BAD_SEARCH_AUTH);
    // Invalidate the access token and retry a delayed search on access errors.
    access_token_.clear();
    search_retry_timer_.Stop();
    base::TimeDelta delay = std::max(base::TimeDelta::FromHours(kDelayHours),
                                     search_retry_timer_.GetCurrentDelay() * 2);
    search_retry_timer_.Start(FROM_HERE, delay, this,
                              &ChromeToMobileService::RequestDeviceSearch);
  } else {
    LogMetric(BAD_SEARCH_OTHER);
  }

  // Update the cached mobile device list in profile prefs.
  profile_->GetPrefs()->Set(prefs::kChromeToMobileDeviceList, mobiles);
  if (HasMobiles())
    LogMetric(DEVICES_AVAILABLE);
  UpdateCommandState();
}

void ChromeToMobileService::HandleSubmitResponse(
    const net::URLFetcher* source) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Get the success value from the cloud print server response data.
  std::string data;
  bool success = false;
  source->GetResponseAsString(&data);
  DictionaryValue* dictionary = NULL;
  scoped_ptr<Value> json(base::JSONReader::Read(data));
  if (json.get() && json->GetAsDictionary(&dictionary) && dictionary) {
    dictionary->GetBoolean("success", &success);
    int error_code = -1;
    if (dictionary->GetInteger("errorCode", &error_code))
      LogMetric(error_code == 407 ? BAD_SEND_407 : BAD_SEND_ERROR);
  } else if (source->GetResponseCode() == net::HTTP_FORBIDDEN) {
    LogMetric(BAD_SEND_AUTH);
  } else {
    LogMetric(BAD_SEND_OTHER);
  }

  // Log each URL and [DELAYED_]SNAPSHOT job submission response.
  LogMetric(success ? SEND_SUCCESS : SEND_ERROR);
  LOG_IF(INFO, !success) << "ChromeToMobile send failed (" <<
                            source->GetResponseCode() << "): " << data;

  // Get the observer for this job submission response.
  base::WeakPtr<Observer> observer;
  RequestObserverMap::iterator i = request_observer_map_.find(source);
  if (i != request_observer_map_.end()) {
    observer = i->second;
    request_observer_map_.erase(i);
  }

  // Check if the observer is waiting on a second response (url or snapshot).
  for (RequestObserverMap::iterator other = request_observer_map_.begin();
       observer.get() && (other != request_observer_map_.end()); ++other) {
    if (other->second == observer) {
      // Delay reporting success until the second response is received.
      if (success)
        return;

      // Report failure below and ignore the second response.
      request_observer_map_.erase(other);
      break;
    }
  }

  if (observer.get())
    observer->OnSendComplete(success);
}
