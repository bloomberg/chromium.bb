// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/net_internals_ui.h"

#include <sstream>

#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/url_request_context_getter.h"
#include "chrome/browser/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"

namespace {

// Formats |t| as a decimal number, in milliseconds.
std::string TickCountToString(const base::TimeTicks& t) {
  return Int64ToString((t - base::TimeTicks()).InMilliseconds());
}

// Returns the HostCache for |context|'s primary HostResolver, or NULL if
// there is none.
net::HostCache* GetHostResolverCache(URLRequestContext* context) {
  net::HostResolverImpl* host_resolver_impl =
      context->host_resolver()->GetAsHostResolverImpl();

  if (!host_resolver_impl)
    return NULL;

  return host_resolver_impl->cache();
}

// TODO(eroman): Bootstrap the net-internals page using the passively logged
//               data.

class NetInternalsHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  NetInternalsHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const;

 private:
  ~NetInternalsHTMLSource() {}
  DISALLOW_COPY_AND_ASSIGN(NetInternalsHTMLSource);
};

// This class receives javascript messages from the renderer.
// Note that the DOMUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
//
// Since the network code we want to run lives on the IO thread, we proxy
// everything over to NetInternalsMessageHandler::IOThreadImpl, which runs
// on the IO thread.
//
// TODO(eroman): Can we start on the IO thread to begin with?
class NetInternalsMessageHandler
    : public DOMMessageHandler,
      public base::SupportsWeakPtr<NetInternalsMessageHandler> {
 public:
  NetInternalsMessageHandler();
  virtual ~NetInternalsMessageHandler();

  // DOMMessageHandler implementation.
  virtual DOMMessageHandler* Attach(DOMUI* dom_ui);
  virtual void RegisterMessages();

  // Executes the javascript function |function_name| in the renderer, passing
  // it the argument |value|.
  void CallJavascriptFunction(const std::wstring& function_name,
                              const Value& value);

 private:
  class IOThreadImpl;

  // This is the "real" message handler, which lives on the IO thread.
  scoped_refptr<IOThreadImpl> proxy_;

  DISALLOW_COPY_AND_ASSIGN(NetInternalsMessageHandler);
};

// This class is the "real" message handler. With the exception of being
// allocated and destroyed on the UI thread, its methods are expected to be
// called from the IO thread.
class NetInternalsMessageHandler::IOThreadImpl
    : public base::RefCountedThreadSafe<
          NetInternalsMessageHandler::IOThreadImpl,
          ChromeThread::DeleteOnUIThread>,
      public ChromeNetLog::Observer {
 public:
  // Type for methods that can be used as MessageHandler callbacks.
  typedef void (IOThreadImpl::*MessageHandler)(const Value*);

  // Creates a proxy for |handler| that will live on the IO thread.
  // |handler| is a weak pointer, since it is possible for the DOMMessageHandler
  // to be deleted on the UI thread while we were executing on the IO thread.
  // |io_thread| is the global IOThread (it is passed in as an argument since
  // we need to grab it from the UI thread).
  IOThreadImpl(
      const base::WeakPtr<NetInternalsMessageHandler>& handler,
      IOThread* io_thread,
      URLRequestContextGetter* context_getter);

  ~IOThreadImpl();

  // Creates a callback that will run |method| on the IO thread.
  //
  // This can be used with DOMUI::RegisterMessageCallback() to bind to a method
  // on the IO thread.
  DOMUI::MessageCallback* CreateCallback(MessageHandler method);

  // Called once the DOMUI has been deleted (i.e. renderer went away), on the
  // IO thread.
  void Detach();

  //--------------------------------
  // Javascript message handlers:
  //--------------------------------

  // This message is called after the webpage's onloaded handler has fired.
  // it indicates that the renderer is ready to start receiving captured data.
  void OnRendererReady(const Value* value);

  void OnGetProxySettings(const Value* value);
  void OnReloadProxySettings(const Value* value);
  void OnGetBadProxies(const Value* value);
  void OnClearBadProxies(const Value* value);
  void OnGetHostResolverCache(const Value* value);
  void OnClearHostResolverCache(const Value* value);

  // ChromeNetLog::Observer implementation:
  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* extra_parameters);

 private:
  class CallbackHelper;

  // Helper that runs |method| with |arg|, and deletes |arg| on completion.
  void DispatchToMessageHandler(Value* arg, MessageHandler method);

  // Helper that executes |function_name| in the attached renderer.
  // The function takes ownership of |arg|.
  void CallJavascriptFunction(const std::wstring& function_name,
                              Value* arg);

  // Pointer to the UI-thread message handler. Only access this from
  // the UI thread.
  base::WeakPtr<NetInternalsMessageHandler> handler_;

  // The global IOThread, which contains the global NetLog to observer.
  IOThread* io_thread_;

  scoped_refptr<URLRequestContextGetter> context_getter_;

  // True if we have attached an observer to the NetLog already.
  bool is_observing_log_;
  friend class base::RefCountedThreadSafe<IOThreadImpl>;
};

// Helper class for a DOMUI::MessageCallback which when excuted calls
// instance->*method(value) on the IO thread.
class NetInternalsMessageHandler::IOThreadImpl::CallbackHelper
    : public DOMUI::MessageCallback {
 public:
  CallbackHelper(IOThreadImpl* instance, IOThreadImpl::MessageHandler method)
      : instance_(instance),
        method_(method) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  }

  virtual void RunWithParams(const Tuple1<const Value*>& params) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

    // We need to make a copy of the value in order to pass it over to the IO
    // thread. We will delete this in IOThreadImpl::DispatchMessageHandler().
    Value* value_copy = params.a ? params.a->DeepCopy() : NULL;

    if (!ChromeThread::PostTask(
            ChromeThread::IO, FROM_HERE,
            NewRunnableMethod(instance_.get(),
                              &IOThreadImpl::DispatchToMessageHandler,
                              value_copy, method_))) {
      // Failed posting the task, avoid leaking |value_copy|.
      delete value_copy;
    }
  }

 private:
  scoped_refptr<IOThreadImpl> instance_;
  IOThreadImpl::MessageHandler method_;
};

////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsHTMLSource::NetInternalsHTMLSource()
    : DataSource(chrome::kChromeUINetInternalsHost, MessageLoop::current()) {
}

void NetInternalsHTMLSource::StartDataRequest(const std::string& path,
                                              bool is_off_the_record,
                                              int request_id) {
  // The provided |path| identifies a file in resources/net_internals/.
  std::string data_string;
  FilePath file_path;
  PathService::Get(chrome::DIR_NET_INTERNALS, &file_path);
  std::string filename;

  // The provided "path" may contain a fragment, or query section. We only
  // care about the path itself, and will disregard anything else.
  filename = GURL(std::string("chrome://net/") + path).path().substr(1);

  if (filename.empty())
    filename = "index.html";

  file_path = file_path.AppendASCII(filename);

  if (!file_util::ReadFileToString(file_path, &data_string)) {
    LOG(WARNING) << "Could not read resource: " << file_path.value();
    data_string = StringPrintf(
        "Failed to read file RESOURCES/net_internals/%s",
        filename.c_str());
  }

  scoped_refptr<RefCountedBytes> bytes(new RefCountedBytes);
  bytes->data.resize(data_string.size());
  std::copy(data_string.begin(), data_string.end(), bytes->data.begin());

  SendResponse(request_id, bytes);
}

std::string NetInternalsHTMLSource::GetMimeType(const std::string&) const {
  // TODO(eroman): This is incorrect -- some of the subresources may be
  //               css/javascript.
  return "text/html";
}

////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsMessageHandler::NetInternalsMessageHandler() {}

NetInternalsMessageHandler::~NetInternalsMessageHandler() {
  if (proxy_) {
    // Notify the handler on the IO thread that the renderer is gone.
    ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(proxy_.get(), &IOThreadImpl::Detach));
  }
}

DOMMessageHandler* NetInternalsMessageHandler::Attach(DOMUI* dom_ui) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  proxy_ = new IOThreadImpl(this->AsWeakPtr(), g_browser_process->io_thread(),
                            dom_ui->GetProfile()->GetRequestContext());
  DOMMessageHandler* result = DOMMessageHandler::Attach(dom_ui);
  return result;
}

void NetInternalsMessageHandler::RegisterMessages() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  dom_ui_->RegisterMessageCallback("notifyReady",
      proxy_->CreateCallback(&IOThreadImpl::OnRendererReady));
  dom_ui_->RegisterMessageCallback("getProxySettings",
      proxy_->CreateCallback(&IOThreadImpl::OnGetProxySettings));
  dom_ui_->RegisterMessageCallback("reloadProxySettings",
      proxy_->CreateCallback(&IOThreadImpl::OnReloadProxySettings));
  dom_ui_->RegisterMessageCallback("getBadProxies",
      proxy_->CreateCallback(&IOThreadImpl::OnGetBadProxies));
  dom_ui_->RegisterMessageCallback("clearBadProxies",
      proxy_->CreateCallback(&IOThreadImpl::OnClearBadProxies));
  dom_ui_->RegisterMessageCallback("getHostResolverCache",
      proxy_->CreateCallback(&IOThreadImpl::OnGetHostResolverCache));
  dom_ui_->RegisterMessageCallback("clearHostResolverCache",
      proxy_->CreateCallback(&IOThreadImpl::OnClearHostResolverCache));
}

void NetInternalsMessageHandler::CallJavascriptFunction(
    const std::wstring& function_name,
    const Value& value) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  dom_ui_->CallJavascriptFunction(function_name, value);
}

////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsMessageHandler::IOThreadImpl
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsMessageHandler::IOThreadImpl::IOThreadImpl(
    const base::WeakPtr<NetInternalsMessageHandler>& handler,
    IOThread* io_thread,
    URLRequestContextGetter* context_getter)
    : handler_(handler),
      io_thread_(io_thread),
      context_getter_(context_getter),
      is_observing_log_(false) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

NetInternalsMessageHandler::IOThreadImpl::~IOThreadImpl() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

DOMUI::MessageCallback*
NetInternalsMessageHandler::IOThreadImpl::CreateCallback(
    MessageHandler method) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  return new CallbackHelper(this, method);
}

void NetInternalsMessageHandler::IOThreadImpl::Detach() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // Unregister with network stack to observe events.
  if (is_observing_log_)
    io_thread_->globals()->net_log->RemoveObserver(this);
}

void NetInternalsMessageHandler::IOThreadImpl::OnRendererReady(
    const Value* value) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(!is_observing_log_) << "notifyReady called twice";

  // Register with network stack to observe events.
  is_observing_log_ = true;
  io_thread_->globals()->net_log->AddObserver(this);

  // Tell the javascript about the relationship between event type enums and
  // their symbolic name.
  {
    std::vector<net::NetLog::EventType> event_types =
        net::NetLog::GetAllEventTypes();

    DictionaryValue* dict = new DictionaryValue();

    for (size_t i = 0; i < event_types.size(); ++i) {
      const char* name = net::NetLog::EventTypeToString(event_types[i]);
      dict->SetInteger(ASCIIToWide(name),
                       static_cast<int>(event_types[i]));
    }

    CallJavascriptFunction(L"g_browser.receivedLogEventTypeConstants", dict);
  }

  // Tell the javascript about the relationship between event phase enums and
  // their symbolic name.
  {
    DictionaryValue* dict = new DictionaryValue();

    dict->SetInteger(L"PHASE_BEGIN", net::NetLog::PHASE_BEGIN);
    dict->SetInteger(L"PHASE_END", net::NetLog::PHASE_END);
    dict->SetInteger(L"PHASE_NONE", net::NetLog::PHASE_NONE);

    CallJavascriptFunction(L"g_browser.receivedLogEventPhaseConstants", dict);
  }

  // Tell the javascript about the relationship between source type enums and
  // their symbolic name.
  // TODO(eroman): Don't duplicate the values, it will never stay up to date!
  {
    DictionaryValue* dict = new DictionaryValue();

    dict->SetInteger(L"NONE", net::NetLog::SOURCE_NONE);
    dict->SetInteger(L"URL_REQUEST", net::NetLog::SOURCE_URL_REQUEST);
    dict->SetInteger(L"SOCKET_STREAM", net::NetLog::SOURCE_SOCKET_STREAM);
    dict->SetInteger(L"INIT_PROXY_RESOLVER",
                     net::NetLog::SOURCE_INIT_PROXY_RESOLVER);
    dict->SetInteger(L"CONNECT_JOB", net::NetLog::SOURCE_CONNECT_JOB);

    CallJavascriptFunction(L"g_browser.receivedLogSourceTypeConstants", dict);
  }

  // Tell the javascript how the "time ticks" values we have given it relate to
  // actual system times. (We used time ticks throughout since they are stable
  // across system clock changes).
  {
    int64 cur_time_ms = (base::Time::Now() - base::Time()).InMilliseconds();

    int64 cur_time_ticks_ms =
        (base::TimeTicks::Now() - base::TimeTicks()).InMilliseconds();

    // If we add this number to a time tick value, it gives the timestamp.
    int64 tick_to_time_ms = cur_time_ms - cur_time_ticks_ms;

    // Chrome on all platforms stores times using the Windows epoch
    // (Jan 1 1601), but the javascript wants a unix epoch.
    // TODO(eroman): Getting the timestamp relative the to unix epoch should
    //               be part of the time library.
    const int64 kUnixEpochMs = 11644473600000LL;
    int64 tick_to_unix_time_ms = tick_to_time_ms - kUnixEpochMs;

    // Pass it as a string, since it may be too large to fit in an integer.
    CallJavascriptFunction(L"g_browser.receivedTimeTickOffset",
                           Value::CreateStringValue(
                               Int64ToString(tick_to_unix_time_ms)));
  }

  // Notify the client of the basic proxy data.
  OnGetProxySettings(NULL);
  OnGetBadProxies(NULL);
  OnGetHostResolverCache(NULL);
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetProxySettings(
    const Value* value) {
  URLRequestContext* context = context_getter_->GetURLRequestContext();
  net::ProxyService* proxy_service = context->proxy_service();

  // TODO(eroman): send a dictionary rather than a flat string, so client can do
  //               its own presentation.
  std::string settings_string;

  if (proxy_service->config_has_been_initialized()) {
    // net::ProxyConfig defines an operator<<.
    std::ostringstream stream;
    stream << proxy_service->config();
    settings_string = stream.str();
  }

  CallJavascriptFunction(L"g_browser.receivedProxySettings",
                         Value::CreateStringValue(settings_string));
}

void NetInternalsMessageHandler::IOThreadImpl::OnReloadProxySettings(
    const Value* value) {
  URLRequestContext* context = context_getter_->GetURLRequestContext();
  context->proxy_service()->ForceReloadProxyConfig();

  // Cause the renderer to be notified of the new values.
  OnGetProxySettings(NULL);
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetBadProxies(
    const Value* value) {
  URLRequestContext* context = context_getter_->GetURLRequestContext();

  const net::ProxyRetryInfoMap& bad_proxies_map =
      context->proxy_service()->proxy_retry_info();

  ListValue* list = new ListValue();

  for (net::ProxyRetryInfoMap::const_iterator it = bad_proxies_map.begin();
       it != bad_proxies_map.end(); ++it) {
    const std::string& proxy_uri = it->first;
    const net::ProxyRetryInfo& retry_info = it->second;

    DictionaryValue* dict = new DictionaryValue();
    dict->SetString(L"proxy_uri", proxy_uri);
    dict->SetString(L"bad_until", TickCountToString(retry_info.bad_until));

    list->Append(dict);
  }

  CallJavascriptFunction(L"g_browser.receivedBadProxies", list);
}

void NetInternalsMessageHandler::IOThreadImpl::OnClearBadProxies(
    const Value* value) {
  URLRequestContext* context = context_getter_->GetURLRequestContext();
  context->proxy_service()->ClearBadProxiesCache();

  // Cause the renderer to be notified of the new values.
  OnGetBadProxies(NULL);
}

void NetInternalsMessageHandler::IOThreadImpl::OnGetHostResolverCache(
    const Value* value) {

  net::HostCache* cache =
      GetHostResolverCache(context_getter_->GetURLRequestContext());

  if (!cache) {
    CallJavascriptFunction(L"g_browser.receivedHostResolverCache", NULL);
    return;
  }

  DictionaryValue* dict = new DictionaryValue();

  dict->SetInteger(L"capacity", static_cast<int>(cache->max_entries()));
  dict->SetInteger(
      L"ttl_success_ms",
      static_cast<int>(cache->success_entry_ttl().InMilliseconds()));
  dict->SetInteger(
      L"ttl_failure_ms",
      static_cast<int>(cache->failure_entry_ttl().InMilliseconds()));

  ListValue* entry_list = new ListValue();

  for (net::HostCache::EntryMap::const_iterator it =
       cache->entries().begin();
       it != cache->entries().end();
       ++it) {
    const net::HostCache::Key& key = it->first;
    const net::HostCache::Entry* entry = it->second.get();

    DictionaryValue* entry_dict = new DictionaryValue();

    entry_dict->SetString(L"hostname", key.hostname);
    entry_dict->SetInteger(L"address_family",
        static_cast<int>(key.address_family));
    entry_dict->SetString(L"expiration", TickCountToString(entry->expiration));

    if (entry->error != net::OK) {
      entry_dict->SetInteger(L"error", entry->error);
    } else {
      // Append all of the resolved addresses.
      ListValue* address_list = new ListValue();
      const struct addrinfo* current_address = entry->addrlist.head();
      while (current_address) {
        address_list->Append(Value::CreateStringValue(
            net::NetAddressToString(current_address)));
        current_address = current_address->ai_next;
      }
      entry_dict->Set(L"addresses", address_list);
    }

    entry_list->Append(entry_dict);
  }

  dict->Set(L"entries", entry_list);

  CallJavascriptFunction(L"g_browser.receivedHostResolverCache", dict);
}

void NetInternalsMessageHandler::IOThreadImpl::OnClearHostResolverCache(
    const Value* value) {
  net::HostCache* cache =
      GetHostResolverCache(context_getter_->GetURLRequestContext());

  if (cache)
    cache->clear();

  // Cause the renderer to be notified of the new values.
  OnGetHostResolverCache(NULL);
}

void NetInternalsMessageHandler::IOThreadImpl::OnAddEntry(
    net::NetLog::EventType type,
    const base::TimeTicks& time,
    const net::NetLog::Source& source,
    net::NetLog::EventPhase phase,
    net::NetLog::EventParameters* extra_parameters) {
  DCHECK(is_observing_log_);

  // JSONify the NetLog::Entry.
  // TODO(eroman): Need a better format for this.
  DictionaryValue* entry_dict = new DictionaryValue();

  // Set the entry time. (Note that we send it as a string since integers
  // might overflow).
  entry_dict->SetString(L"time", TickCountToString(time));

  // Set the entry source.
  DictionaryValue* source_dict = new DictionaryValue();
  source_dict->SetInteger(L"id", source.id);
  source_dict->SetInteger(L"type", static_cast<int>(source.type));
  entry_dict->Set(L"source", source_dict);

  // Set the event info.
  entry_dict->SetInteger(L"type", static_cast<int>(type));
  entry_dict->SetInteger(L"phase", static_cast<int>(phase));

  // Set the event-specific parameters.
  if (extra_parameters)
    entry_dict->SetString(L"extra_parameters", extra_parameters->ToString());

  CallJavascriptFunction(L"g_browser.receivedLogEntry", entry_dict);
}

void NetInternalsMessageHandler::IOThreadImpl::DispatchToMessageHandler(
    Value* arg, MessageHandler method) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  (this->*method)(arg);
  delete arg;
}

void NetInternalsMessageHandler::IOThreadImpl::CallJavascriptFunction(
    const std::wstring& function_name,
    Value* arg) {
  if (ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    if (handler_) {
      // We check |handler_| in case it was deleted on the UI thread earlier
      // while we were running on the IO thread.
      handler_->CallJavascriptFunction(function_name, *arg);
    }
    delete arg;
    return;
  }


  // Otherwise if we were called from the IO thread, bridge the request over to
  // the UI thread.

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!ChromeThread::PostTask(
           ChromeThread::UI, FROM_HERE,
           NewRunnableMethod(
               this,
               &IOThreadImpl::CallJavascriptFunction,
               function_name, arg))) {
    // Failed posting the task, avoid leaking.
    delete arg;
  }
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// NetInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

NetInternalsUI::NetInternalsUI(TabContents* contents) : DOMUI(contents) {
  AddMessageHandler((new NetInternalsMessageHandler())->Attach(this));

  NetInternalsHTMLSource* html_source = new NetInternalsHTMLSource();

  // Set up the chrome://net-internals/ source.
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(html_source)));
}
