// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/web_resource_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/web_resource/web_resource_unpacker.h"
#include "content/browser/utility_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

// This class coordinates a web resource unpack and parse task which is run in
// a separate process.  Results are sent back to this class and routed to
// the WebResourceService.
class WebResourceService::UnpackerClient : public UtilityProcessHost::Client {
 public:
  explicit UnpackerClient(WebResourceService* web_resource_service)
    : web_resource_service_(web_resource_service),
      resource_dispatcher_host_(g_browser_process->resource_dispatcher_host()),
      got_response_(false) {
  }

  void Start(const std::string& json_data) {
    AddRef();  // balanced in Cleanup.

    // TODO(willchan): Look for a better signal of whether we're in a unit test
    // or not. Using |resource_dispatcher_host_| for this is pretty lame.
    // If we don't have a resource_dispatcher_host_, assume we're in
    // a test and run the unpacker directly in-process.
    bool use_utility_process =
        resource_dispatcher_host_ != NULL &&
        !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess);
    if (use_utility_process) {
      BrowserThread::ID thread_id;
      CHECK(BrowserThread::GetCurrentThreadIdentifier(&thread_id));
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&UnpackerClient::StartProcessOnIOThread,
                     this, thread_id, json_data));
    } else {
      WebResourceUnpacker unpacker(json_data);
      if (unpacker.Run()) {
        OnUnpackWebResourceSucceeded(*unpacker.parsed_json());
      } else {
        OnUnpackWebResourceFailed(unpacker.error_message());
      }
    }
  }

 private:
  virtual ~UnpackerClient() {}

  // UtilityProcessHost::Client
  virtual bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(WebResourceService::UnpackerClient, message)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_UnpackWebResource_Succeeded,
                          OnUnpackWebResourceSucceeded)
      IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_UnpackWebResource_Failed,
                          OnUnpackWebResourceFailed)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  virtual void OnProcessCrashed(int exit_code) {
    if (got_response_)
      return;

    OnUnpackWebResourceFailed(
        "Utility process crashed while trying to retrieve web resources.");
  }

  void OnUnpackWebResourceSucceeded(
      const DictionaryValue& parsed_json) {
    web_resource_service_->Unpack(parsed_json);
    Cleanup();
  }

  void OnUnpackWebResourceFailed(const std::string& error_message) {
    LOG(ERROR) << error_message;
    Cleanup();
  }

  // Release reference and set got_response_.
  void Cleanup() {
    DCHECK(!got_response_);
    got_response_ = true;

    web_resource_service_->EndFetch();
    Release();
  }

  void StartProcessOnIOThread(BrowserThread::ID thread_id,
                              const std::string& json_data) {
    UtilityProcessHost* host = new UtilityProcessHost(this, thread_id);
    host->set_use_linux_zygote(true);
    // TODO(mrc): get proper file path when we start using web resources
    // that need to be unpacked.
    host->Send(new ChromeUtilityMsg_UnpackWebResource(json_data));
  }

  scoped_refptr<WebResourceService> web_resource_service_;

  // Owned by the global browser process.
  ResourceDispatcherHost* resource_dispatcher_host_;

  // True if we got a response from the utility process and have cleaned up
  // already.
  bool got_response_;
};

WebResourceService::WebResourceService(
    PrefService* prefs,
    const char* web_resource_server,
    bool apply_locale_to_url,
    const char* last_update_time_pref_name,
    int start_fetch_delay_ms,
    int cache_update_delay_ms)
    : prefs_(prefs),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      in_fetch_(false),
      web_resource_server_(web_resource_server),
      apply_locale_to_url_(apply_locale_to_url),
      last_update_time_pref_name_(last_update_time_pref_name),
      start_fetch_delay_ms_(start_fetch_delay_ms),
      cache_update_delay_ms_(cache_update_delay_ms) {
  DCHECK(prefs);
}

WebResourceService::~WebResourceService() { }

void WebResourceService::EndFetch() {
  in_fetch_ = false;
}

void WebResourceService::StartAfterDelay() {
  int64 delay = start_fetch_delay_ms_;
  // Check whether we have ever put a value in the web resource cache;
  // if so, pull it out and see if it's time to update again.
  if (prefs_->HasPrefPath(last_update_time_pref_name_)) {
    std::string last_update_pref =
        prefs_->GetString(last_update_time_pref_name_);
    if (!last_update_pref.empty()) {
      double last_update_value;
      base::StringToDouble(last_update_pref, &last_update_value);
      int64 ms_until_update = cache_update_delay_ms_ -
          static_cast<int64>((base::Time::Now() - base::Time::FromDoubleT(
          last_update_value)).InMilliseconds());
      // Wait at least |start_fetch_delay_ms_|.
      if (ms_until_update > start_fetch_delay_ms_)
        delay = ms_until_update;
    }
  }
  // Start fetch and wait for UpdateResourceCache.
  ScheduleFetch(delay);
}

// Delay initial load of resource data into cache so as not to interfere
// with startup time.
void WebResourceService::ScheduleFetch(int64 delay_ms) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebResourceService::StartFetch,
                 weak_ptr_factory_.GetWeakPtr()),
      delay_ms);
}

// Initializes the fetching of data from the resource server.  Data
// load calls OnURLFetchComplete.
void WebResourceService::StartFetch() {
  // First, put our next cache load on the MessageLoop.
  ScheduleFetch(cache_update_delay_ms_);

  // Set cache update time in preferences.
  prefs_->SetString(last_update_time_pref_name_,
      base::DoubleToString(base::Time::Now().ToDoubleT()));

  // If we are still fetching data, exit.
  if (in_fetch_)
    return;
  in_fetch_ = true;

  // Balanced in OnURLFetchComplete.
  AddRef();

  std::string web_resource_server = web_resource_server_;
  if (apply_locale_to_url_) {
    std::string locale = g_browser_process->GetApplicationLocale();
    web_resource_server.append(locale);
  }

  url_fetcher_.reset(content::URLFetcher::Create(
      GURL(web_resource_server), content::URLFetcher::GET, this));
  // Do not let url fetcher affect existing state in system context
  // (by setting cookies, for example).
  url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE |
                             net::LOAD_DO_NOT_SAVE_COOKIES);
  net::URLRequestContextGetter* url_request_context_getter =
      g_browser_process->system_request_context();
  url_fetcher_->SetRequestContext(url_request_context_getter);
  url_fetcher_->Start();
}

void WebResourceService::OnURLFetchComplete(const content::URLFetcher* source) {
  // Delete the URLFetcher when this function exits.
  scoped_ptr<content::URLFetcher> clean_up_fetcher(url_fetcher_.release());

  // Don't parse data if attempt to download was unsuccessful.
  // Stop loading new web resource data, and silently exit.
  if (!source->GetStatus().is_success() || (source->GetResponseCode() != 200))
    return;

  std::string data;
  source->GetResponseAsString(&data);

  // UnpackerClient releases itself.
  UnpackerClient* client = new UnpackerClient(this);
  client->Start(data);

  Release();
}
