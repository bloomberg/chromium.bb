// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/intents_dispatcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/common/intents_messages.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSerializedScriptValue.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"
#include "webkit/glue/cpp_bound_class.h"

using WebKit::WebCString;
using WebKit::WebString;

// This class encapsulates the API the Intent object will expose to Javascript.
// It is made available to the Javascript runtime in the service page using
// NPAPI methods as with plugin/Javascript interaction objects and other
// browser-provided Javascript API objects on |window|.
class IntentsDispatcher::BoundDeliveredIntent : public CppBoundClass {
 public:
  BoundDeliveredIntent(const string16& action,
                       const string16& type,
                       const string16& data,
                       IntentsDispatcher* parent,
                       WebKit::WebFrame* frame) {
    action_ = WebString(action).utf8();
    type_ = WebString(type).utf8();
    parent_ = parent;

    v8::HandleScope scope;
    v8::Local<v8::Context> ctx = frame->mainWorldScriptContext();
    v8::Context::Scope cscope(ctx);
    WebKit::WebSerializedScriptValue ssv =
        WebKit::WebSerializedScriptValue::fromString(WebString(data));
    // TODO(gbillock): use an exception handler instead? Need to
    // pass back error state to caller? This is a pretty unexpected
    // internal error...
    CHECK(!ssv.isNull());
    v8::Local<v8::Value> data_obj =
        v8::Local<v8::Value>::New(ssv.deserialize());

    data_val_.reset(new CppVariant);
    WebKit::WebBindings::toNPVariant(data_obj,
                                     frame->windowObject(),
                                     data_val_.get());

    BindGetterCallback("action", base::Bind(&BoundDeliveredIntent::getAction,
                                            base::Unretained(this)));
    BindGetterCallback("type", base::Bind(&BoundDeliveredIntent::getType,
                                          base::Unretained(this)));
    BindGetterCallback("data", base::Bind(&BoundDeliveredIntent::getData,
                                          base::Unretained(this)));
    BindCallback("postResult", base::Bind(&BoundDeliveredIntent::postResult,
                                          base::Unretained(this)));
    BindCallback("postFailure", base::Bind(&BoundDeliveredIntent::postFailure,
                                           base::Unretained(this)));
  }

  virtual ~BoundDeliveredIntent() {
  }

  WebKit::WebString SerializeCppVariant(const CppVariant& val) {
    v8::HandleScope scope;
    v8::Handle<v8::Value> v8obj = WebKit::WebBindings::toV8Value(&val);

    WebKit::WebSerializedScriptValue ssv =
        WebKit::WebSerializedScriptValue::serialize(v8obj);
    if (ssv.isNull())
      return WebKit::WebString();

    return ssv.toString();
  }

  void postResult(const CppArgumentList& args, CppVariant* retval) {
    if (args.size() != 1) {
      WebKit::WebBindings::setException(
          NULL, "Must pass one argument to postResult");
      return;
    }

    WebString str = SerializeCppVariant(args[0]);
    parent_->OnResult(str);
  }

  void postFailure(const CppArgumentList& args, CppVariant* retval) {
    if (args.size() != 1) {
      WebKit::WebBindings::setException(
          NULL, "Must pass one argument to postFailure");
      return;
    }

    WebString str = SerializeCppVariant(args[0]);
    parent_->OnFailure(str);
  }

  void getAction(CppVariant* result) {
    std::string action;
    action.assign(action_.data(), action_.length());
    result->Set(action);
  }

  void getType(CppVariant* result) {
    std::string type;
    type.assign(type_.data(), type_.length());
    result->Set(type);
  }

  void getData(CppVariant* result) {
    result->Set(*data_val_.get());
  }

 private:
  // Intent data suitable for surfacing to Javascript callers.
  WebCString action_;
  WebCString type_;
  scoped_ptr<CppVariant> data_val_;

  // The dispatcher object, for forwarding postResult/postFailure calls.
  IntentsDispatcher* parent_;
};

IntentsDispatcher::IntentsDispatcher(RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view),
      intent_id_(0) {
}

IntentsDispatcher::~IntentsDispatcher() {}

bool IntentsDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(IntentsDispatcher, message)
    IPC_MESSAGE_HANDLER(IntentsMsg_SetWebIntentData, OnSetIntent)
    IPC_MESSAGE_HANDLER(IntentsMsg_WebIntentReply, OnWebIntentReply);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IntentsDispatcher::OnSetIntent(const webkit_glue::WebIntentData& intent,
                                    int intent_id) {
  intent_.reset(new webkit_glue::WebIntentData(intent));
  intent_id_ = intent_id;
}

void IntentsDispatcher::OnWebIntentReply(
    webkit_glue::WebIntentReplyType reply_type,
    const WebKit::WebString& data,
    int intent_id) {
  LOG(INFO) << "RenderView got reply to intent type " << reply_type;

  if (reply_type == webkit_glue::WEB_INTENT_REPLY_SUCCESS) {
    render_view()->GetWebView()->mainFrame()->handleIntentResult(
        intent_id, data);
  } else {
    render_view()->GetWebView()->mainFrame()->handleIntentFailure(
        intent_id, data);
  }
}

void IntentsDispatcher::OnResult(const WebKit::WebString& data) {
  Send(new IntentsMsg_WebIntentReply(
      routing_id(),
      webkit_glue::WEB_INTENT_REPLY_SUCCESS,
      data,
      intent_id_));
}

void IntentsDispatcher::OnFailure(const WebKit::WebString& data) {
  Send(new IntentsMsg_WebIntentReply(
      routing_id(),
      webkit_glue::WEB_INTENT_REPLY_FAILURE,
      data,
      intent_id_));
}

// We set the intent payload into all top-level frame window objects. This
// should persist the data through redirects, and not deliver it to any
// sub-frames. TODO(gbillock): This policy needs to be fine-tuned and
// documented.
void IntentsDispatcher::DidClearWindowObject(WebKit::WebFrame* frame) {
  if (intent_.get() == NULL || frame->top() != frame)
    return;

  delivered_intent_.reset(new BoundDeliveredIntent(
      intent_->action, intent_->type, intent_->data, this, frame));
  delivered_intent_->BindToJavascript(frame, "intent");
}
