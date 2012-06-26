// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_intents_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "content/common/intents_messages.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBlob.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeliveredIntentClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntentRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebFileSystem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSerializedScriptValue.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "v8/include/v8.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/glue/cpp_bound_class.h"

using WebKit::WebBindings;
using WebKit::WebBlob;
using WebKit::WebCString;
using WebKit::WebDeliveredIntentClient;
using WebKit::WebFrame;
using WebKit::WebIntent;
using WebKit::WebIntentRequest;
using WebKit::WebString;
using WebKit::WebSerializedScriptValue;
using WebKit::WebVector;

class DeliveredIntentClientImpl : public WebDeliveredIntentClient {
 public:
  explicit DeliveredIntentClientImpl(WebIntentsHost* host) : host_(host) {}
  virtual ~DeliveredIntentClientImpl() {}

  virtual void postResult(const WebSerializedScriptValue& data) const {
    host_->OnResult(data.toString());
  }

  virtual void postFailure(const WebSerializedScriptValue& data) const {
    host_->OnFailure(data.toString());
  }

  virtual void destroy() OVERRIDE {
  }

 private:
  WebIntentsHost* host_;
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
// sub-frames.
// TODO(gbillock): match to spec to double-check registration match before
// delivery.
void WebIntentsHost::DidClearWindowObject(WebFrame* frame) {
  if (intent_.get() == NULL || frame->top() != frame)
    return;

  if (!delivered_intent_client_.get()) {
    delivered_intent_client_.reset(new DeliveredIntentClientImpl(this));
  }

  WebVector<WebString> extras_keys(intent_->extra_data.size());
  WebVector<WebString> extras_values(intent_->extra_data.size());
  std::map<string16, string16>::iterator iter;
  int i;
  for (i = 0, iter = intent_->extra_data.begin();
       iter != intent_->extra_data.end();
       ++i, ++iter) {
    extras_keys[i] = iter->first;
    extras_values[i] = iter->second;
  }

  v8::HandleScope scope;
  v8::Local<v8::Context> ctx = frame->mainWorldScriptContext();
  v8::Context::Scope cscope(ctx);
  WebIntent web_intent;

  if (intent_->data_type == webkit_glue::WebIntentData::SERIALIZED) {
    web_intent = WebIntent::create(intent_->action, intent_->type,
                                   intent_->data,
                                   extras_keys, extras_values);
  } else if (intent_->data_type == webkit_glue::WebIntentData::UNSERIALIZED) {
    v8::Local<v8::String> dataV8 = v8::String::New(
        reinterpret_cast<const uint16_t*>(intent_->unserialized_data.data()),
        static_cast<int>(intent_->unserialized_data.length()));
    WebSerializedScriptValue serialized_data =
        WebSerializedScriptValue::serialize(dataV8);

    web_intent = WebIntent::create(intent_->action, intent_->type,
                                   serialized_data.toString(),
                                   extras_keys, extras_values);
  } else if (intent_->data_type == webkit_glue::WebIntentData::BLOB) {
    DCHECK(intent_->data_type == webkit_glue::WebIntentData::BLOB);
    web_blob_ = WebBlob::createFromFile(
        WebString::fromUTF8(intent_->blob_file.AsUTF8Unsafe()),
        intent_->blob_length);
    WebSerializedScriptValue serialized_data =
        WebSerializedScriptValue::serialize(web_blob_.toV8Value());
    web_intent = WebIntent::create(intent_->action, intent_->type,
                                   serialized_data.toString(),
                                   extras_keys, extras_values);
  } else if (intent_->data_type == webkit_glue::WebIntentData::FILESYSTEM) {
    const GURL origin = GURL(frame->document().securityOrigin().toString());
    const GURL root_url =
        fileapi::GetFileSystemRootURI(origin, fileapi::kFileSystemTypeIsolated);
    const std::string fsname =
        fileapi::GetIsolatedFileSystemName(origin, intent_->filesystem_id);
    const std::string url = base::StringPrintf(
        "%s%s/%s/",
        root_url.spec().c_str(),
        intent_->filesystem_id.c_str(),
        intent_->root_path.BaseName().AsUTF8Unsafe().c_str());
    // TODO(kmadhusu): This is a temporary hack to create a serializable file
    // system. Once we have a better way to create a serializable file system,
    // remove this hack.
    v8::Handle<v8::Value> filesystem_V8 = frame->createSerializableFileSystem(
        WebKit::WebFileSystem::TypeIsolated,
        WebKit::WebString::fromUTF8(fsname),
        WebKit::WebString::fromUTF8(url));
    WebSerializedScriptValue serialized_data =
        WebSerializedScriptValue::serialize(filesystem_V8);
    web_intent = WebIntent::create(intent_->action, intent_->type,
                                   serialized_data.toString(),
                                   extras_keys, extras_values);
  } else {
    NOTREACHED();
  }

  if (!web_intent.action().isEmpty())
    frame->deliverIntent(web_intent, NULL, delivered_intent_client_.get());
}
