// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/net_internals_ui.h"

#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"

namespace {

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
      IOThread* io_thread);

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

  // ChromeNetLog::Observer implementation:
  virtual void OnAddEntry(const net::NetLog::Entry& entry);

 private:
  class CallbackHelper;

  // Helper that runs |method| with |arg|, and deletes |arg| on completion.
  void DispatchToMessageHandler(Value* arg, MessageHandler method);

  // Helper that executes |function_name| in the attached renderer.
  // The function takes ownership of |arg|.
  void CallJavascriptFunction(const std::wstring& function_name,
                              Value* arg);

 private:
  // Pointer to the UI-thread message handler. Only access this from
  // the UI thread.
  base::WeakPtr<NetInternalsMessageHandler> handler_;

  // The global IOThread, which contains the global NetLog to observer.
  IOThread* io_thread_;

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
  std::string filename = path.empty() ? "index.html" : path;
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
  proxy_ = new IOThreadImpl(this->AsWeakPtr(), g_browser_process->io_thread());
  DOMMessageHandler* result = DOMMessageHandler::Attach(dom_ui);
  return result;
}

void NetInternalsMessageHandler::RegisterMessages() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  dom_ui_->RegisterMessageCallback("notifyReady",
      proxy_->CreateCallback(&IOThreadImpl::OnRendererReady));
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
    IOThread* io_thread)
    : handler_(handler),
      io_thread_(io_thread),
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

    CallJavascriptFunction(L"setLogEventTypeConstants", dict);
  }

  // Tell the javascript about the relationship between event phase enums and
  // their symbolic name.
  {
    DictionaryValue* dict = new DictionaryValue();

    dict->SetInteger(L"PHASE_BEGIN", net::NetLog::PHASE_BEGIN);
    dict->SetInteger(L"PHASE_END", net::NetLog::PHASE_END);
    dict->SetInteger(L"PHASE_NONE", net::NetLog::PHASE_NONE);

    CallJavascriptFunction(L"setLogEventPhaseConstants", dict);
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

    CallJavascriptFunction(L"setLogSourceTypeConstants", dict);
  }

  // Tell the javascript about the relationship between entry type enums and
  // their symbolic name.
  {
    DictionaryValue* dict = new DictionaryValue();

    dict->SetInteger(L"TYPE_EVENT", net::NetLog::Entry::TYPE_EVENT);
    dict->SetInteger(L"TYPE_STRING", net::NetLog::Entry::TYPE_STRING);
    dict->SetInteger(L"TYPE_ERROR_CODE", net::NetLog::Entry::TYPE_ERROR_CODE);

    CallJavascriptFunction(L"setLogEntryTypeConstants", dict);
  }
}

void NetInternalsMessageHandler::IOThreadImpl::OnAddEntry(
    const net::NetLog::Entry& entry) {
  DCHECK(is_observing_log_);

  // JSONify the NetLog::Entry.
  // TODO(eroman): Need a better format for this.
  DictionaryValue* entry_dict = new DictionaryValue();

  // Set the entry type.
  {
    net::NetLog::Entry::Type entry_type = entry.type;
    if (entry_type == net::NetLog::Entry::TYPE_STRING_LITERAL)
      entry_type = net::NetLog::Entry::TYPE_STRING;
    entry_dict->SetInteger(L"type", static_cast<int>(entry_type));
  }

  // Set the entry time.
  entry_dict->SetInteger(
      L"time",
      static_cast<int>((entry.time - base::TimeTicks()).InMilliseconds()));

  // Set the entry source.
  DictionaryValue* source_dict = new DictionaryValue();
  source_dict->SetInteger(L"id", entry.source.id);
  source_dict->SetInteger(L"type", static_cast<int>(entry.source.type));
  entry_dict->Set(L"source", source_dict);

  // Set the event info (if it is an event entry).
  if (entry.type == net::NetLog::Entry::TYPE_EVENT) {
    DictionaryValue* event_dict = new DictionaryValue();
    event_dict->SetInteger(L"type", static_cast<int>(entry.event.type));
    event_dict->SetInteger(L"phase", static_cast<int>(entry.event.phase));
    entry_dict->Set(L"event", event_dict);
  }

  // Add the string information (events my have a string too, due to current
  // hacks).
  if (entry.type == net::NetLog::Entry::TYPE_STRING || !entry.string.empty()) {
    entry_dict->SetString(L"string", entry.string);
  }

  // Treat string literals the same as strings.
  if (entry.type == net::NetLog::Entry::TYPE_STRING_LITERAL) {
    entry_dict->SetString(L"string", entry.literal);
  }

  if (entry.type == net::NetLog::Entry::TYPE_ERROR_CODE) {
    entry_dict->SetInteger(L"error_code", entry.error_code);
  }

  CallJavascriptFunction(L"onLogEntryAdded", entry_dict);
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
