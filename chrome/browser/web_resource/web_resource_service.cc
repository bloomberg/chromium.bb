// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/web_resource_service.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/web_resource/web_resource_unpacker.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

class WebResourceService::WebResourceFetcher
    : public URLFetcher::Delegate {
 public:
  explicit WebResourceFetcher(WebResourceService* web_resource_service) :
      ALLOW_THIS_IN_INITIALIZER_LIST(fetcher_factory_(this)),
      web_resource_service_(web_resource_service) {
  }

  // Delay initial load of resource data into cache so as not to interfere
  // with startup time.
  void StartAfterDelay(int64 delay_ms) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        fetcher_factory_.NewRunnableMethod(&WebResourceFetcher::StartFetch),
                                           delay_ms);
  }

  // Initializes the fetching of data from the resource server.  Data
  // load calls OnURLFetchComplete.
  void StartFetch() {
    // Balanced in OnURLFetchComplete.
    web_resource_service_->AddRef();
    // First, put our next cache load on the MessageLoop.
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        fetcher_factory_.NewRunnableMethod(&WebResourceFetcher::StartFetch),
            web_resource_service_->cache_update_delay_);
    // If we are still fetching data, exit.
    if (web_resource_service_->in_fetch_)
      return;
    else
      web_resource_service_->in_fetch_ = true;

    std::string web_resource_server =
        web_resource_service_->web_resource_server_;
    if (web_resource_service_->apply_locale_to_url_) {
      std::string locale = g_browser_process->GetApplicationLocale();
      web_resource_server.append(locale);
    }

    url_fetcher_.reset(new URLFetcher(GURL(
        web_resource_server),
        URLFetcher::GET, this));
    // Do not let url fetcher affect existing state in profile (by setting
    // cookies, for example.
    url_fetcher_->set_load_flags(net::LOAD_DISABLE_CACHE |
        net::LOAD_DO_NOT_SAVE_COOKIES);
    net::URLRequestContextGetter* url_request_context_getter =
        web_resource_service_->profile_->GetRequestContext();
    url_fetcher_->set_request_context(url_request_context_getter);
    url_fetcher_->Start();
  }

  // From URLFetcher::Delegate.
  void OnURLFetchComplete(const URLFetcher* source,
                          const GURL& url,
                          const net::URLRequestStatus& status,
                          int response_code,
                          const ResponseCookies& cookies,
                          const std::string& data) {
    // Delete the URLFetcher when this function exits.
    scoped_ptr<URLFetcher> clean_up_fetcher(url_fetcher_.release());

    // Don't parse data if attempt to download was unsuccessful.
    // Stop loading new web resource data, and silently exit.
    if (!status.is_success() || (response_code != 200))
      return;

    web_resource_service_->UpdateResourceCache(data);
    web_resource_service_->Release();
  }

 private:
  // So that we can delay our start so as not to affect start-up time; also,
  // so that we can schedule future cache updates.
  ScopedRunnableMethodFactory<WebResourceFetcher> fetcher_factory_;

  // The tool that fetches the url data from the server.
  scoped_ptr<URLFetcher> url_fetcher_;

  // Our owner and creator. Ref counted.
  WebResourceService* web_resource_service_;
};

// This class coordinates a web resource unpack and parse task which is run in
// a separate process.  Results are sent back to this class and routed to
// the WebResourceService.
class WebResourceService::UnpackerClient
    : public UtilityProcessHost::Client {
 public:
  UnpackerClient(WebResourceService* web_resource_service,
                 const std::string& json_data)
    : web_resource_service_(web_resource_service),
      json_data_(json_data), got_response_(false) {
  }

  void Start() {
    AddRef();  // balanced in Cleanup.

    // TODO(willchan): Look for a better signal of whether we're in a unit test
    // or not. Using |resource_dispatcher_host_| for this is pretty lame.
    // If we don't have a resource_dispatcher_host_, assume we're in
    // a test and run the unpacker directly in-process.
    bool use_utility_process =
        web_resource_service_->resource_dispatcher_host_ != NULL &&
        !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
    if (use_utility_process) {
      BrowserThread::ID thread_id;
      CHECK(BrowserThread::GetCurrentThreadIdentifier(&thread_id));
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(this, &UnpackerClient::StartProcessOnIOThread,
                            thread_id));
    } else {
      WebResourceUnpacker unpacker(json_data_);
      if (unpacker.Run()) {
        OnUnpackWebResourceSucceeded(*unpacker.parsed_json());
      } else {
        OnUnpackWebResourceFailed(unpacker.error_message());
      }
    }
  }

 private:
  ~UnpackerClient() {}

  // UtilityProcessHost::Client
  virtual void OnProcessCrashed(int exit_code) {
    if (got_response_)
      return;

    OnUnpackWebResourceFailed(
        "Chrome crashed while trying to retrieve web resources.");
  }

  virtual void OnUnpackWebResourceSucceeded(
      const DictionaryValue& parsed_json) {
    web_resource_service_->OnWebResourceUnpacked(parsed_json);
    Cleanup();
  }

  virtual void OnUnpackWebResourceFailed(const std::string& error_message) {
    web_resource_service_->EndFetch();
    Cleanup();
  }

  // Release reference and set got_response_.
  void Cleanup() {
    if (got_response_)
      return;

    got_response_ = true;
    Release();
  }

  void StartProcessOnIOThread(BrowserThread::ID thread_id) {
    UtilityProcessHost* host = new UtilityProcessHost(this, thread_id);
    // TODO(mrc): get proper file path when we start using web resources
    // that need to be unpacked.
    host->StartWebResourceUnpacker(json_data_);
  }

  scoped_refptr<WebResourceService> web_resource_service_;

  // Holds raw JSON string.
  const std::string& json_data_;

  // True if we got a response from the utility process and have cleaned up
  // already.
  bool got_response_;
};

WebResourceService::WebResourceService(
    Profile* profile,
    PrefService* prefs,
    const char* web_resource_server,
    bool apply_locale_to_url,
    NotificationType::Type notification_type,
    const char* last_update_time_pref_name,
    int start_fetch_delay,
    int cache_update_delay)
    : prefs_(prefs),
      profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(service_factory_(this)),
      in_fetch_(false),
      web_resource_server_(web_resource_server),
      apply_locale_to_url_(apply_locale_to_url),
      notification_type_(notification_type),
      last_update_time_pref_name_(last_update_time_pref_name),
      start_fetch_delay_(start_fetch_delay),
      cache_update_delay_(cache_update_delay),
      web_resource_update_scheduled_(false) {
  DCHECK(prefs);
  DCHECK(profile);
  prefs_->RegisterStringPref(last_update_time_pref_name, "0");
  resource_dispatcher_host_ = g_browser_process->resource_dispatcher_host();
  web_resource_fetcher_.reset(new WebResourceFetcher(this));
}

WebResourceService::~WebResourceService() { }

void WebResourceService::PostNotification(int64 delay_ms) {
  if (web_resource_update_scheduled_)
    return;
  if (delay_ms > 0) {
    web_resource_update_scheduled_ = true;
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        service_factory_.NewRunnableMethod(
            &WebResourceService::WebResourceStateChange), delay_ms);
  } else if (delay_ms == 0) {
    WebResourceStateChange();
  }
}

void WebResourceService::EndFetch() {
  in_fetch_ = false;
}

void WebResourceService::OnWebResourceUnpacked(
  const DictionaryValue& parsed_json) {
  Unpack(parsed_json);
  EndFetch();
}

void WebResourceService::WebResourceStateChange() {
  web_resource_update_scheduled_ = false;
  if (notification_type_ == NotificationType::NOTIFICATION_TYPE_COUNT)
    return;
  NotificationService* service = NotificationService::current();
  service->Notify(notification_type_,
                  Source<WebResourceService>(this),
                  NotificationService::NoDetails());
}

void WebResourceService::StartAfterDelay() {
  int64 delay = start_fetch_delay_;
  // Check whether we have ever put a value in the web resource cache;
  // if so, pull it out and see if it's time to update again.
  if (prefs_->HasPrefPath(last_update_time_pref_name_)) {
    std::string last_update_pref =
        prefs_->GetString(last_update_time_pref_name_);
    if (!last_update_pref.empty()) {
      double last_update_value;
      base::StringToDouble(last_update_pref, &last_update_value);
      int64 ms_until_update = cache_update_delay_ -
          static_cast<int64>((base::Time::Now() - base::Time::FromDoubleT(
          last_update_value)).InMilliseconds());
      delay = ms_until_update > cache_update_delay_ ?
          cache_update_delay_ : (ms_until_update < start_fetch_delay_ ?
                                start_fetch_delay_ : ms_until_update);
    }
  }
  // Start fetch and wait for UpdateResourceCache.
  web_resource_fetcher_->StartAfterDelay(delay);
}

void WebResourceService::UpdateResourceCache(const std::string& json_data) {
  UnpackerClient* client = new UnpackerClient(this, json_data);
  client->Start();

  // Set cache update time in preferences.
  prefs_->SetString(last_update_time_pref_name_,
      base::DoubleToString(base::Time::Now().ToDoubleT()));
}
