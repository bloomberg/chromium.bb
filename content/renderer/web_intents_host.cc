// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_intents_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/common/intents_messages.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntentRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSerializedScriptValue.h"
#include "v8/include/v8.h"
#include "webkit/glue/cpp_bound_class.h"

using WebKit::WebBindings;
using WebKit::WebCString;
using WebKit::WebFrame;
using WebKit::WebIntentRequest;
using WebKit::WebString;
using WebKit::WebSerializedScriptValue;

// This class encapsulates the API the Intent object will expose to Javascript.
// It is made available to the Javascript runtime in the service page using
// NPAPI methods as with plugin/Javascript interaction objects and other
// browser-provided Javascript API objects on |window|.
class WebIntentsHost::BoundDeliveredIntent : public CppBoundClass {
 public:
  BoundDeliveredIntent(const webkit_glue::WebIntentData& intent,
                       WebIntentsHost* parent,
                       WebFrame* frame) {
    action_ = WebString(intent.action).utf8();
    type_ = WebString(intent.type).utf8();
    parent_ = parent;

    v8::HandleScope scope;
    v8::Local<v8::Context> ctx = frame->mainWorldScriptContext();
    v8::Context::Scope cscope(ctx);
    v8::Local<v8::Value> data_obj;

    if (intent.data_type == webkit_glue::WebIntentData::SERIALIZED) {
      WebSerializedScriptValue ssv =
          WebSerializedScriptValue::fromString(WebString(intent.data));
      DCHECK(!ssv.isNull());
      data_obj = v8::Local<v8::Value>::New(ssv.deserialize());
    } else {
      DCHECK(intent.data_type == webkit_glue::WebIntentData::UNSERIALIZED);
      data_obj = v8::String::New(
          reinterpret_cast<const uint16_t*>(intent.unserialized_data.data()),
          static_cast<int>(intent.unserialized_data.length()));
    }

    data_val_.reset(new CppVariant);
    WebBindings::toNPVariant(data_obj, frame->windowObject(), data_val_.get());

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

  WebString SerializeCppVariant(const CppVariant& val) {
    v8::HandleScope scope;
    v8::Handle<v8::Value> v8obj = WebBindings::toV8Value(&val);

    WebSerializedScriptValue ssv =
        WebSerializedScriptValue::serialize(v8obj);
    if (ssv.isNull())
      return WebKit::WebString();

    return ssv.toString();
  }

  void postResult(const CppArgumentList& args, CppVariant* retval) {
    if (args.size() != 1) {
      WebBindings::setException(NULL, "Must pass one argument to postResult");
      return;
    }

    WebString str = SerializeCppVariant(args[0]);
    parent_->OnResult(str);
  }

  void postFailure(const CppArgumentList& args, CppVariant* retval) {
    if (args.size() != 1) {
      WebBindings::setException(NULL, "Must pass one argument to postFailure");
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
  WebIntentsHost* parent_;
};

WebIntentsHost::WebIntentsHost(RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view),
      id_counter_(0) {
}

WebIntentsHost::~WebIntentsHost() {
}

int WebIntentsHost::RegisterWebIntent(
    const WebIntentRequest& request) {
  int id = id_counter_++;
  intent_requests_[id] = request;
  return id;
}

bool WebIntentsHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebIntentsHost, message)
    IPC_MESSAGE_HANDLER(IntentsMsg_SetWebIntentData, OnSetIntent)
    IPC_MESSAGE_HANDLER(IntentsMsg_WebIntentReply, OnWebIntentReply);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebIntentsHost::OnSetIntent(const webkit_glue::WebIntentData& intent) {
  intent_.reset(new webkit_glue::WebIntentData(intent));
}

void WebIntentsHost::OnWebIntentReply(
    webkit_glue::WebIntentReplyType reply_type,
    const WebKit::WebString& data,
    int intent_id) {
  std::map<int, WebIntentRequest>::iterator request =
      intent_requests_.find(intent_id);
  if (request == intent_requests_.end())
    return;
  WebIntentRequest intent_request = request->second;
  intent_requests_.erase(request);
  WebSerializedScriptValue value =
      WebSerializedScriptValue::fromString(data);

  if (reply_type == webkit_glue::WEB_INTENT_REPLY_SUCCESS) {
    intent_request.postResult(value);
  } else {
    intent_request.postFailure(value);
  }
}

void WebIntentsHost::OnResult(const WebKit::WebString& data) {
  Send(new IntentsHostMsg_WebIntentReply(
      routing_id(), webkit_glue::WEB_INTENT_REPLY_SUCCESS, data));
}

void WebIntentsHost::OnFailure(const WebKit::WebString& data) {
  Send(new IntentsHostMsg_WebIntentReply(
      routing_id(), webkit_glue::WEB_INTENT_REPLY_FAILURE, data));
}

// We set the intent payload into all top-level frame window objects. This
// should persist the data through redirects, and not deliver it to any
// sub-frames. TODO(gbillock): This policy needs to be fine-tuned and
// documented.
void WebIntentsHost::DidClearWindowObject(WebFrame* frame) {
  if (intent_.get() == NULL || frame->top() != frame)
    return;

  delivered_intent_.reset(
      new BoundDeliveredIntent(*(intent_.get()), this, frame));
  delivered_intent_->BindToJavascript(frame, "webkitIntent");
}
