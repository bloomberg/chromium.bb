// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/net_internals_ui.h"

#include "app/resource_bundle.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/url_constants.h"

#include "grit/browser_resources.h"

namespace {

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
          ChromeThread::DeleteOnUIThread> {
 public:
  // Type for methods that can be used as MessageHandler callbacks.
  typedef void (IOThreadImpl::*MessageHandler)(const Value*);

  // Creates a proxy for |handler| that will live on the IO thread.
  // |handler| is a weak pointer, since it is possible for the DOMMessageHandler
  // to be deleted on the UI thread while we were executing on the IO thread.
  explicit IOThreadImpl(
      const base::WeakPtr<NetInternalsMessageHandler>& handler);

  ~IOThreadImpl();

  // Creates a callback that will run |method| on the IO thread.
  //
  // This can be used with DOMUI::RegisterMessageCallback() to bind to a method
  // on the IO thread.
  DOMUI::MessageCallback* CreateCallback(MessageHandler method);

  // Called once the DOMUI has attached to the renderer, on the IO thread.
  void Attach();

  // Called once the DOMUI has been deleted (i.e. renderer went away), on the
  // IO thread.
  void Detach();

  //--------------------------------
  // Javascript message handlers:
  //--------------------------------

  // TODO(eroman): This is temporary!
  void OnTestMessage(const Value* value);

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
  // Serve up the HTML contained in the resource bundle.
  base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_NET_INTERNALS_HTML));

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
  html_bytes->data.resize(html.size());
  std::copy(html.begin(), html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

std::string NetInternalsHTMLSource::GetMimeType(const std::string&) const {
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
  proxy_ = new IOThreadImpl(this->AsWeakPtr());

  DOMMessageHandler* result = DOMMessageHandler::Attach(dom_ui);

  // Notify the handler on the IO thread that a renderer is attached.
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(proxy_.get(), &IOThreadImpl::Attach));

  return result;
}

void NetInternalsMessageHandler::RegisterMessages() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // TODO(eroman): Register message handlers here.
  dom_ui_->RegisterMessageCallback("testMessage",
      proxy_->CreateCallback(&IOThreadImpl::OnTestMessage));
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
    const base::WeakPtr<NetInternalsMessageHandler>& handler)
    : handler_(handler) {
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

void NetInternalsMessageHandler::IOThreadImpl::Attach() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // TODO(eroman): Register with network stack to observe events.
}

void NetInternalsMessageHandler::IOThreadImpl::Detach() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // TODO(eroman): Unregister with network stack to observe events.
}

void NetInternalsMessageHandler::IOThreadImpl::OnTestMessage(
    const Value* value) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // TODO(eroman): This is just a temporary method, to see something in
  //               action. We expect to have been called with an array
  //               containing 1 string, and print it to the screen.
  std::string str;
  if (value && value->GetType() == Value::TYPE_LIST) {
    const ListValue* list_value = static_cast<const ListValue*>(value);
    Value* list_member;
    if (list_value->Get(0, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      string_value->GetAsString(&str);
    }
  }

  CallJavascriptFunction(
      L"log",
      Value::CreateStringValue("Browser received testMessage: " + str));
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
