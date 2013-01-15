// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/web_intents_host.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/utf_string_conversions.h"
#include "content/common/intents_messages.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBlob.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeliveredIntentClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntentRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSerializedScriptValue.h"
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

namespace content {

namespace {

// Reads reply value, either from the data field, or the data_file field.
WebSerializedScriptValue GetReplyValue(
    const webkit_glue::WebIntentReply& reply) {
  if (reply.data_file_size > -1) {
    // TODO(smckay): seralize the blob value in the web intent script
    // context. We simply don't know how to do this at this time.
    // The following code kills the renderer when we call toV8Value.
    //    v8::HandleScope scope;
    //    v8::Local<v8::Context> ctx = ....which context?
    //    v8::Context::Scope cscope(ctx);
    //    WebKit::WebBlob blob = WebBlob::createFromFile(
    //        WebString::fromUTF8(reply.data_file.AsUTF8Unsafe()),
    //        reply.data_file_size);
    //    v8::Handle<v8::Value> value = blob.toV8Value();
    //    return WebSerializedScriptValue::serialize(value);
    return WebSerializedScriptValue::fromString(
        ASCIIToUTF16(reply.data_file.AsUTF8Unsafe()));
  } else {
    return WebSerializedScriptValue::fromString(reply.data);
  }
}

}  // namespace

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
    : RenderViewObserver(render_view),
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
    const webkit_glue::WebIntentReply& reply,
    int intent_id) {
  std::map<int, WebIntentRequest>::iterator request =
      intent_requests_.find(intent_id);
  if (request == intent_requests_.end())
    return;
  WebIntentRequest intent_request = request->second;
  intent_requests_.erase(request);

  if (reply.type == webkit_glue::WEB_INTENT_REPLY_SUCCESS) {
    intent_request.postResult(GetReplyValue(reply));
  } else {
    intent_request.postFailure(
        WebSerializedScriptValue::fromString(reply.data));
  }
}

void WebIntentsHost::OnResult(const WebKit::WebString& data) {
  const webkit_glue::WebIntentReply reply(
      webkit_glue::WEB_INTENT_REPLY_SUCCESS, data);
  Send(new IntentsHostMsg_WebIntentReply(routing_id(), reply));
}

void WebIntentsHost::OnFailure(const WebKit::WebString& data) {
  const webkit_glue::WebIntentReply reply(
      webkit_glue::WEB_INTENT_REPLY_FAILURE, data);
  Send(new IntentsHostMsg_WebIntentReply(routing_id(), reply));
}

// We set the intent payload into all top-level frame window objects. This
// should persist the data through redirects, and not deliver it to any
// sub-frames.
void WebIntentsHost::DidCreateScriptContext(WebKit::WebFrame* frame,
                                            v8::Handle<v8::Context> ctx,
                                            int extension_group,
                                            int world_id) {
  if (intent_.get() == NULL || frame->top() != frame)
    return;

  if (ctx != frame->mainWorldScriptContext())
    return;

  if (!delivered_intent_client_.get()) {
    delivered_intent_client_.reset(new DeliveredIntentClientImpl(this));
  }

  WebIntent web_intent = CreateWebIntent(frame, *intent_);

  if (!web_intent.action().isEmpty())
    frame->deliverIntent(web_intent, NULL, delivered_intent_client_.get());
}

WebIntent WebIntentsHost::CreateWebIntent(
    WebFrame* frame, const webkit_glue::WebIntentData& intent_data) {
  // Must be called with v8 scope held.
  DCHECK(v8::Context::InContext());

  // TODO(gbillock): Remove this block when we get rid of |extras|.
  WebVector<WebString> extras_keys(intent_data.extra_data.size());
  WebVector<WebString> extras_values(intent_data.extra_data.size());
  std::map<string16, string16>::const_iterator iter =
      intent_data.extra_data.begin();
  for (size_t i = 0; iter != intent_data.extra_data.end(); ++i, ++iter) {
    extras_keys[i] = iter->first;
    extras_values[i] = iter->second;
  }

  switch (intent_data.data_type) {
    case webkit_glue::WebIntentData::SERIALIZED: {
      return WebIntent::create(intent_data.action, intent_data.type,
                               intent_data.data,
                               extras_keys, extras_values);
    }

    case webkit_glue::WebIntentData::UNSERIALIZED: {
      v8::Local<v8::String> dataV8 = v8::String::New(
          reinterpret_cast<const uint16_t*>(
              intent_data.unserialized_data.data()),
          static_cast<int>(intent_data.unserialized_data.length()));
      WebSerializedScriptValue serialized_data =
          WebSerializedScriptValue::serialize(dataV8);

      return WebIntent::create(intent_data.action, intent_data.type,
                               serialized_data.toString(),
                               extras_keys, extras_values);
    }

    case webkit_glue::WebIntentData::BLOB: {
      web_blob_ = WebBlob::createFromFile(
          WebString::fromUTF8(intent_data.blob_file.AsUTF8Unsafe()),
          intent_data.blob_length);
      WebSerializedScriptValue serialized_data =
          WebSerializedScriptValue::serialize(web_blob_.toV8Value());
      return WebIntent::create(intent_data.action, intent_data.type,
                               serialized_data.toString(),
                               extras_keys, extras_values);
    }

    case webkit_glue::WebIntentData::FILESYSTEM: {
      const GURL origin = GURL(frame->document().securityOrigin().toString());
      const GURL root_url =
          fileapi::GetFileSystemRootURI(origin,
                                        fileapi::kFileSystemTypeIsolated);
      const std::string url = base::StringPrintf(
          "%s%s/%s/",
          root_url.spec().c_str(),
          intent_data.filesystem_id.c_str(),
          intent_data.root_name.c_str());
      // TODO(kmadhusu): This is a temporary hack to create a serializable file
      // system. Once we have a better way to create a serializable file system,
      // remove this hack.
      v8::Handle<v8::Value> filesystem_V8 = frame->createSerializableFileSystem(
          WebKit::WebFileSystem::TypeIsolated,
          WebKit::WebString::fromUTF8(intent_data.root_name),
          WebKit::WebString::fromUTF8(url));
      WebSerializedScriptValue serialized_data =
          WebSerializedScriptValue::serialize(filesystem_V8);
      return WebIntent::create(intent_data.action, intent_data.type,
                               serialized_data.toString(),
                               extras_keys, extras_values);
    }

    case webkit_glue::WebIntentData::MIME_TYPE: {
      scoped_ptr<V8ValueConverter> converter(
          V8ValueConverter::create());
      v8::Handle<v8::Value> valV8 = converter->ToV8Value(
          &intent_data.mime_data, v8::Context::GetCurrent());

      WebSerializedScriptValue serialized_data =
          WebSerializedScriptValue::serialize(valV8);
      return WebIntent::create(intent_data.action, intent_data.type,
                               serialized_data.toString(),
                               WebVector<WebString>(), WebVector<WebString>());
    }
  }

  NOTREACHED();
  return WebIntent();
}

}  // namespace content
