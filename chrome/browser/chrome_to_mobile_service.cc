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
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
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
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

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

// The cloud print OAuth2 scope and 'printer' type of compatible mobile devices.
const char kCloudPrintAuth[] = "https://www.googleapis.com/auth/cloudprint";
const char kTypeAndroidChromeSnapshot[] = "ANDROID_CHROME_SNAPSHOT";
const char kTypeIOSChromeSnapshot[] = "IOS_CHROME_SNAPSHOT";

// The account info URL pattern and strings to check for cloud print access.
// The 'key=' query parameter is used for caching; supply a random number.
// The 'rv=2' query parameter requests a JSON response; use 'rv=1' for XML.
const char kAccountInfoURL[] =
    "https://clients1.google.com/tbproxy/getaccountinfo?key=%s&rv=2&%s";
const char kAccountServicesKey[] = "services";
const char kCloudPrintSerivceValue[] = "cprt";

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
GURL GetSearchURL(const GURL& service_url) {
  GURL search_url = cloud_print::GetUrlForSearch(service_url);
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
  FilePath snapshot;
  bool success = file_util::CreateTemporaryFile(&snapshot);
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(callback, snapshot, success));
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

ChromeToMobileService::ChromeToMobileService(Profile* profile)
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      profile_(profile),
      cloud_print_url_(new CloudPrintURL(profile)),
      cloud_print_accessible_(false) {
  // Skip initialization if constructed without a profile.
  if (profile_) {
    // Get an access token as soon as the Gaia login refresh token is available.
    TokenService* service = TokenServiceFactory::GetForProfile(profile_);
    registrar_.Add(this, chrome::NOTIFICATION_TOKEN_AVAILABLE,
                   content::Source<TokenService>(service));
    if (service->HasOAuthLoginToken())
      RefreshAccessToken();
  }
}

ChromeToMobileService::~ChromeToMobileService() {
  while (!snapshots_.empty())
    DeleteSnapshot(*snapshots_.begin());
}

bool ChromeToMobileService::HasDevices() {
  return !mobiles().empty();
}

const std::vector<base::DictionaryValue*>& ChromeToMobileService::mobiles() {
  return mobiles_.get();
}

void ChromeToMobileService::RequestMobileListUpdate() {
  if (access_token_.empty())
    RefreshAccessToken();
  else if (cloud_print_accessible_)
    RequestSearch();
}

void ChromeToMobileService::GenerateSnapshot(Browser* browser,
                                             base::WeakPtr<Observer> observer) {
  // Callback SnapshotFileCreated from CreateSnapshotFile to continue.
  CreateSnapshotFileCallback callback =
      base::Bind(&ChromeToMobileService::SnapshotFileCreated,
                 weak_ptr_factory_.GetWeakPtr(), observer,
                 browser->session_id().id());
  // Create a temporary file via the blocking pool for snapshot storage.
  content::BrowserThread::PostBlockingPoolTask(FROM_HERE,
      base::Bind(&CreateSnapshotFile, callback));
}

void ChromeToMobileService::SendToMobile(const base::DictionaryValue& mobile,
                                         const FilePath& snapshot,
                                         Browser* browser,
                                         base::WeakPtr<Observer> observer) {
  LogMetric(SENDING_URL);

  DCHECK(!access_token_.empty());
  JobData data;
  std::string mobile_os;
  mobile.GetString("type", &mobile_os);
  data.mobile_os = (mobile_os.compare(kTypeAndroidChromeSnapshot) == 0) ?
      ChromeToMobileService::ANDROID : ChromeToMobileService::IOS;
  mobile.GetString("id", &data.mobile_id);
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
    content::BrowserThread::PostBlockingPoolSequencedTask(
        data.snapshot.AsUTF8Unsafe(), FROM_HERE,
        base::Bind(&ChromeToMobileService::SendRequest,
                   weak_ptr_factory_.GetWeakPtr(), submit_snapshot, data));
  }
}

void ChromeToMobileService::DeleteSnapshot(const FilePath& snapshot) {
  DCHECK(snapshot.empty() || snapshots_.find(snapshot) != snapshots_.end());
  if (snapshots_.find(snapshot) != snapshots_.end()) {
    if (!snapshot.empty())
      content::BrowserThread::PostBlockingPoolSequencedTask(
          snapshot.AsUTF8Unsafe(), FROM_HERE,
          base::Bind(&DeleteSnapshotFile, snapshot));
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

void ChromeToMobileService::OnURLFetchComplete(
    const net::URLFetcher* source) {
  if (source == account_info_request_.get())
    HandleAccountInfoResponse();
  else if (source == search_request_.get())
    HandleSearchResponse();
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
  if (token_details->service() == GaiaConstants::kGaiaOAuth2LoginRefreshToken)
    RefreshAccessToken();
}

void ChromeToMobileService::OnGetTokenSuccess(
    const std::string& access_token) {
  DCHECK(!access_token.empty());
  access_token_fetcher_.reset();
  auth_retry_timer_.Stop();
  access_token_ = access_token;
  RequestAccountInfo();
}

void ChromeToMobileService::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  access_token_fetcher_.reset();
  auth_retry_timer_.Stop();
  auth_retry_timer_.Start(
      FROM_HERE, base::TimeDelta::FromHours(kAuthRetryDelayHours),
      this, &ChromeToMobileService::RefreshAccessToken);
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
  const GURL service_url(cloud_print_url_->GetCloudPrintServiceURL());
  net::URLFetcher* request = net::URLFetcher::Create(
      cloud_print::GetUrlForSubmit(service_url), net::URLFetcher::POST, this);
  InitRequest(request);
  return request;
}

void ChromeToMobileService::InitRequest(net::URLFetcher* request) {
  request->SetRequestContext(profile_->GetRequestContext());
  request->SetMaxRetries(kMaxRetries);
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

void ChromeToMobileService::RefreshAccessToken() {
  if (access_token_fetcher_.get())
    return;

  std::string token = TokenServiceFactory::GetForProfile(profile_)->
                          GetOAuth2LoginRefreshToken();
  if (token.empty())
    return;

  auth_retry_timer_.Stop();
  access_token_fetcher_.reset(
      new OAuth2AccessTokenFetcher(this, profile_->GetRequestContext()));
  std::vector<std::string> scopes(1, kCloudPrintAuth);
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  access_token_fetcher_->Start(gaia_urls->oauth2_chrome_client_id(),
      gaia_urls->oauth2_chrome_client_secret(), token, scopes);
}

void ChromeToMobileService::RequestAccountInfo() {
  // Deny concurrent requests.
  if (account_info_request_.get())
    return;

  std::string url_string = StringPrintf(kAccountInfoURL,
      base::GenerateGUID().c_str(), kChromeToMobileRequestor);
  GURL url(url_string);

  // Account information is read from the profile's cookie. If cookies are
  // blocked, access cloud print directly to list any potential devices.
  scoped_refptr<CookieSettings> cookie_settings =
      CookieSettings::Factory::GetForProfile(profile_);
  if (cookie_settings && !cookie_settings->IsReadingCookieAllowed(url, url)) {
    cloud_print_accessible_ = true;
    RequestMobileListUpdate();
    return;
  }

  account_info_request_.reset(
      net::URLFetcher::Create(url, net::URLFetcher::GET, this));
  account_info_request_->SetRequestContext(profile_->GetRequestContext());
  account_info_request_->SetMaxRetries(kMaxRetries);
  // This request sends the user's cookie to check the cloud print service flag.
  account_info_request_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
  account_info_request_->Start();
}

void ChromeToMobileService::RequestSearch() {
  DCHECK(!access_token_.empty());

  // Deny requests if cloud print is inaccessible, and deny concurrent requests.
  if (!cloud_print_accessible_ || search_request_.get())
    return;

  // Deny requests before the delay period has passed since the last request.
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - previous_search_time_;
  if (!previous_search_time_.is_null() &&
      elapsed_time.InHours() < kSearchRequestDelayHours)
    return;

  LogMetric(DEVICES_REQUESTED);

  const GURL service_url(cloud_print_url_->GetCloudPrintServiceURL());
  search_request_.reset(net::URLFetcher::Create(GetSearchURL(service_url),
                                                net::URLFetcher::GET, this));
  InitRequest(search_request_.get());
  search_request_->Start();
  previous_search_time_ = base::TimeTicks::Now();
}

void ChromeToMobileService::HandleAccountInfoResponse() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::string data;
  account_info_request_->GetResponseAsString(&data);
  account_info_request_.reset();

  ListValue* services = NULL;
  DictionaryValue* dictionary = NULL;
  scoped_ptr<Value> json(base::JSONReader::Read(data));
  StringValue cloud_print_service(kCloudPrintSerivceValue);
  if (json.get() && json->GetAsDictionary(&dictionary) && dictionary &&
      dictionary->GetList(kAccountServicesKey, &services) && services &&
      services->Find(cloud_print_service) != services->end()) {
    cloud_print_accessible_ = true;
    RequestMobileListUpdate();
  }
}

void ChromeToMobileService::HandleSearchResponse() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::string data;
  search_request_->GetResponseAsString(&data);
  search_request_.reset();

  ListValue* list = NULL;
  DictionaryValue* dictionary = NULL;
  scoped_ptr<Value> json(base::JSONReader::Read(data));
  if (json.get() && json->GetAsDictionary(&dictionary) && dictionary &&
      dictionary->GetList(cloud_print::kPrinterListValue, &list)) {
    ScopedVector<base::DictionaryValue> mobiles;
    for (size_t index = 0; index < list->GetSize(); index++) {
      DictionaryValue* mobile_data = NULL;
      if (list->GetDictionary(index, &mobile_data)) {
        std::string mobile_os;
        mobile_data->GetString("type", &mobile_os);
        if (mobile_os.compare(kTypeAndroidChromeSnapshot) == 0 ||
            mobile_os.compare(kTypeIOSChromeSnapshot) == 0) {
          mobiles.push_back(mobile_data->DeepCopy());
        }
      }
    }
    mobiles_ = mobiles.Pass();

    bool found = !mobiles_.empty();
    if (found)
      LogMetric(DEVICES_AVAILABLE);

    for (BrowserList::const_iterator i = BrowserList::begin();
         i != BrowserList::end(); ++i) {
      Browser* browser = *i;
      if (browser->profile() == profile_)
        browser->command_controller()->SendToMobileStateChanged(found);
    }
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
