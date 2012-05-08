// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_to_mobile_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
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
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/cloud_print/cloud_print_helpers.h"
#include "chrome/common/guid.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/net/gaia/oauth2_access_token_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
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
const char kCloudPrintOAuthScope[] =
    "https://www.googleapis.com/auth/cloudprint";
const char kTypeAndroidChromeSnapshot[] = "ANDROID_CHROME_SNAPSHOT";

// The account info URL pattern and strings to check for cloud print access.
// The 'key=' query parameter is used for caching; supply a random number.
// The 'rv=2' query parameter requests a JSON response; use 'rv=1' for XML.
const char kAccountInfoURL[] =
    "https://clients1.google.com/tbproxy/getaccountinfo?key=%s&rv=2&%s";
const char kAccountServicesKey[] = "services";
const char kCloudPrintSerivceValue[] = "cprt";

// The Chrome To Mobile requestor type; used by services for filtering.
const char kChromeToMobileRequestor[] = "requestor=chrome-to-mobile";

// The types of Chrome To Mobile requests sent to the cloud print service.
const char kRequestTypeURL[] = "url";
const char kRequestTypeDelayedSnapshot[] = "url_with_delayed_snapshot";
const char kRequestTypeSnapshot[] = "snapshot";

// Get the "__c2dm__job_data" tag JSON data for the cloud print job submission.
std::string GetJobString(const ChromeToMobileService::RequestData& data) {
  scoped_ptr<DictionaryValue> job(new DictionaryValue());
  job->SetString("url", data.url.spec());
  if (data.type == ChromeToMobileService::URL) {
    job->SetString("type", kRequestTypeURL);
  } else {
    job->SetString("snapID", data.snapshot_id);
    job->SetString("type", (data.type == ChromeToMobileService::SNAPSHOT) ?
        kRequestTypeSnapshot : kRequestTypeDelayedSnapshot);
  }
  std::string job_string;
  base::JSONWriter::Write(job.get(), &job_string);
  return StringPrintf("__c2dm__job_data=%s", job_string.c_str());
}

// Get the URL for cloud print device search; appends a requestor query param.
GURL GetSearchURL(const GURL& service_url) {
  GURL search_url = cloud_print::GetUrlForSearch(service_url);
  GURL::Replacements replacements;
  std::string query(kChromeToMobileRequestor);
  replacements.SetQueryStr(query);
  return search_url.ReplaceComponents(replacements);
}

// Get the URL for cloud print job submission; appends query params if needed.
GURL GetSubmitURL(const GURL& service_url,
                  const ChromeToMobileService::RequestData& data) {
  GURL submit_url = cloud_print::GetUrlForSubmit(service_url);
  if (data.type == ChromeToMobileService::SNAPSHOT)
    return submit_url;

  // Append form data to the URL's query for |URL| and |DELAYED_SNAPSHOT| jobs.
  static const bool kUsePlus = true;
  std::string tag = net::EscapeQueryParamValue(GetJobString(data), kUsePlus);
  // Provide dummy content to workaround |errorCode| 412 'Document missing'.
  std::string query = StringPrintf("printerid=%s&tag=%s&title=%s"
      "&contentType=text/plain&content=dummy",
      net::EscapeQueryParamValue(UTF16ToUTF8(data.mobile_id), kUsePlus).c_str(),
      net::EscapeQueryParamValue(tag, kUsePlus).c_str(),
      net::EscapeQueryParamValue(UTF16ToUTF8(data.title), kUsePlus).c_str());
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return submit_url.ReplaceComponents(replacements);
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

// Send snapshot file contents as POST data in a job submit request.
// Call this as a BlockingPoolSequencedTask (before posting DeleteSnapshotFile).
void SubmitSnapshotFile(content::URLFetcher* request,
                        const ChromeToMobileService::RequestData& data) {
  std::string file;
  if (file_util::ReadFileToString(data.snapshot_path, &file) && !file.empty()) {
    std::string post_data, mime_boundary;
    cloud_print::CreateMimeBoundaryForUpload(&mime_boundary);
    cloud_print::AddMultipartValueForUpload("printerid",
        UTF16ToUTF8(data.mobile_id), mime_boundary, std::string(), &post_data);
    cloud_print::AddMultipartValueForUpload("tag", GetJobString(data),
        mime_boundary, std::string(), &post_data);
    cloud_print::AddMultipartValueForUpload("title", UTF16ToUTF8(data.title),
        mime_boundary, std::string(), &post_data);
    cloud_print::AddMultipartValueForUpload("contentType", "multipart/related",
        mime_boundary, std::string(), &post_data);

    // Append the snapshot MHTML content and terminate the request body.
    post_data.append("--" + mime_boundary + "\r\n"
        "Content-Disposition: form-data; "
        "name=\"content\"; filename=\"blob\"\r\n"
        "Content-Type: text/mhtml\r\n"
        "\r\n" + file + "\r\n" "--" + mime_boundary + "--\r\n");
    std::string content_type = "multipart/form-data; boundary=" + mime_boundary;
    request->SetUploadData(content_type, post_data);
    request->Start();
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

ChromeToMobileService::RequestData::RequestData() {}

ChromeToMobileService::RequestData::~RequestData() {}

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

void ChromeToMobileService::GenerateSnapshot(base::WeakPtr<Observer> observer) {
  // Callback SnapshotFileCreated from CreateSnapshotFile to continue.
  CreateSnapshotFileCallback callback =
      base::Bind(&ChromeToMobileService::SnapshotFileCreated,
                 weak_ptr_factory_.GetWeakPtr(), observer);
  // Create a temporary file via the blocking pool for snapshot storage.
  content::BrowserThread::PostBlockingPoolTask(FROM_HERE,
      base::Bind(&CreateSnapshotFile, callback));
}

void ChromeToMobileService::SendToMobile(const string16& mobile_id,
                                         const FilePath& snapshot,
                                         base::WeakPtr<Observer> observer) {
  LogMetric(SENDING_URL);

  DCHECK(!access_token_.empty());
  RequestData data;
  data.mobile_id = mobile_id;
  content::WebContents* web_contents =
      BrowserList::GetLastActiveWithProfile(profile_)->GetSelectedWebContents();
  data.url = web_contents->GetURL();
  data.title = web_contents->GetTitle();
  data.snapshot_path = snapshot;
  bool send_snapshot = !snapshot.empty();
  data.snapshot_id = send_snapshot ? guid::GenerateGUID() : std::string();
  data.type = send_snapshot ? DELAYED_SNAPSHOT : URL;

  content::URLFetcher* submit_url = CreateRequest(data);
  request_observer_map_[submit_url] = observer;
  submit_url->Start();

  if (send_snapshot) {
    LogMetric(SENDING_SNAPSHOT);

    data.type = SNAPSHOT;
    content::URLFetcher* submit_snapshot = CreateRequest(data);
    request_observer_map_[submit_snapshot] = observer;
    content::BrowserThread::PostBlockingPoolSequencedTask(
        data.snapshot_path.AsUTF8Unsafe(), FROM_HERE,
        base::Bind(&SubmitSnapshotFile, submit_snapshot, data));
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

void ChromeToMobileService::LogMetric(Metric metric) {
  UMA_HISTOGRAM_ENUMERATION("ChromeToMobile.Service", metric, NUM_METRICS);
}

void ChromeToMobileService::OnURLFetchComplete(
    const content::URLFetcher* source) {
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
    const FilePath& path,
    bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Track the set of temporary files to be deleted later.
  snapshots_.insert(path);

  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (success && browser && browser->GetSelectedWebContents()) {
    // Generate the snapshot and have the observer be called back on completion.
    browser->GetSelectedWebContents()->GenerateMHTML(path,
        base::Bind(&Observer::SnapshotGenerated, observer));
  } else if (observer.get()) {
    // Signal snapshot generation failure.
    observer->SnapshotGenerated(FilePath(), 0);
  }
}

content::URLFetcher* ChromeToMobileService::CreateRequest(
  const RequestData& data) {
  bool get = data.type != SNAPSHOT;
  GURL service_url(cloud_print_url_->GetCloudPrintServiceURL());
  content::URLFetcher* request = content::URLFetcher::Create(
      data.type == SEARCH ? GetSearchURL(service_url) :
                            GetSubmitURL(service_url, data),
      get ? content::URLFetcher::GET : content::URLFetcher::POST, this);
  request->SetRequestContext(profile_->GetRequestContext());
  request->SetMaxRetries(kMaxRetries);
  request->SetExtraRequestHeaders("Authorization: OAuth " +
      access_token_ + "\r\n" + cloud_print::kChromeCloudPrintProxyHeader);
  request->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                        net::LOAD_DO_NOT_SAVE_COOKIES);
  return request;
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
  std::vector<std::string> scopes(1, kCloudPrintOAuthScope);
  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  access_token_fetcher_->Start(gaia_urls->oauth2_chrome_client_id(),
      gaia_urls->oauth2_chrome_client_secret(), token, scopes);
}

void ChromeToMobileService::RequestAccountInfo() {
  // Deny concurrent requests.
  if (account_info_request_.get())
    return;

  std::string url_string = StringPrintf(kAccountInfoURL,
      guid::GenerateGUID().c_str(), kChromeToMobileRequestor);
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
      content::URLFetcher::Create(url, content::URLFetcher::GET, this));
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

  RequestData data;
  data.type = SEARCH;
  search_request_.reset(CreateRequest(data));
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
        std::string mobile_type;
        mobile_data->GetString("type", &mobile_type);
        if (mobile_type.compare(kTypeAndroidChromeSnapshot) == 0)
          mobiles.push_back(mobile_data->DeepCopy());
      }
    }
    mobiles_ = mobiles.Pass();

    if (!mobiles_.empty())
      LogMetric(DEVICES_AVAILABLE);

    Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
    if (browser && browser->command_updater())
      browser->command_updater()->UpdateCommandEnabled(
          IDC_CHROME_TO_MOBILE_PAGE, !mobiles_.empty());
  }
}

void ChromeToMobileService::HandleSubmitResponse(
    const content::URLFetcher* source) {
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
