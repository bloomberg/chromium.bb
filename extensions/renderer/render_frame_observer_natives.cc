// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/render_frame_observer_natives.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

namespace {

// Deletes itself when done.
class LoadWatcher : public content::RenderFrameObserver {
 public:
  LoadWatcher(ScriptContext* context,
              content::RenderFrame* frame,
              v8::Local<v8::Function> cb)
      : content::RenderFrameObserver(frame),
        context_(context),
        callback_(context->isolate(), cb) {
    if (ExtensionFrameHelper::Get(frame)->
            did_create_current_document_element()) {
      // If the document element is already created, then we can call the
      // callback immediately (though post it to the message loop so as to not
      // call it re-entrantly).
      // The Unretained is safe because this class manages its own lifetime.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&LoadWatcher::CallbackAndDie, base::Unretained(this),
                     true));
    }
  }

  void DidCreateDocumentElement() override { CallbackAndDie(true); }

  void DidFailProvisionalLoad(const blink::WebURLError& error) override {
    CallbackAndDie(false);
  }

 private:
  void CallbackAndDie(bool succeeded) {
    v8::Isolate* isolate = context_->isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> args[] = {v8::Boolean::New(isolate, succeeded)};
    context_->CallFunction(v8::Local<v8::Function>::New(isolate, callback_),
                           arraysize(args), args);
    delete this;
  }

  ScriptContext* context_;
  v8::Global<v8::Function> callback_;

  DISALLOW_COPY_AND_ASSIGN(LoadWatcher);
};

}  // namespace

RenderFrameObserverNatives::RenderFrameObserverNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "OnDocumentElementCreated",
      base::Bind(&RenderFrameObserverNatives::OnDocumentElementCreated,
                 base::Unretained(this)));
}

void RenderFrameObserverNatives::OnDocumentElementCreated(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args.Length() == 2);
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsFunction());

  int frame_id = args[0]->Int32Value();

  content::RenderFrame* frame = content::RenderFrame::FromRoutingID(frame_id);
  if (!frame) {
    LOG(WARNING) << "No render frame found to register LoadWatcher.";
    return;
  }

  new LoadWatcher(context(), frame, args[1].As<v8::Function>());

  args.GetReturnValue().Set(true);
}

}  // namespace extensions
