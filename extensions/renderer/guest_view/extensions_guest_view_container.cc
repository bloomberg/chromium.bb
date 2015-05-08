// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/extensions_guest_view_container.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebScopedMicrotaskSuppression.h"
#include "ui/gfx/geometry/size.h"

namespace extensions {

ExtensionsGuestViewContainer::ExtensionsGuestViewContainer(
    content::RenderFrame* render_frame)
    : GuestViewContainer(render_frame),
      destruction_isolate_(nullptr),
      element_resize_isolate_(nullptr),
      weak_ptr_factory_(this) {
}

ExtensionsGuestViewContainer::~ExtensionsGuestViewContainer() {
  // Call the destruction callback, if one is registered.
  if (!destruction_callback_.IsEmpty()) {
    v8::HandleScope handle_scope(destruction_isolate_);
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::New(
        destruction_isolate_, destruction_callback_);
    v8::Local<v8::Context> context = callback->CreationContext();
    if (context.IsEmpty())
      return;

    v8::Context::Scope context_scope(context);
    blink::WebScopedMicrotaskSuppression suppression;

    callback->Call(context->Global(), 0 /* argc */, nullptr);
  }
}

void ExtensionsGuestViewContainer::RegisterDestructionCallback(
    v8::Local<v8::Function> callback,
    v8::Isolate* isolate) {
  destruction_callback_.Reset(isolate, callback);
  destruction_isolate_ = isolate;
}

void ExtensionsGuestViewContainer::RegisterElementResizeCallback(
    v8::Local<v8::Function> callback,
    v8::Isolate* isolate) {
  element_resize_callback_.Reset(isolate, callback);
  element_resize_isolate_ = isolate;
}

void ExtensionsGuestViewContainer::DidResizeElement(const gfx::Size& old_size,
                                                    const gfx::Size& new_size) {
  // Call the element resize callback, if one is registered.
  if (element_resize_callback_.IsEmpty())
    return;

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ExtensionsGuestViewContainer::CallElementResizeCallback,
                 weak_ptr_factory_.GetWeakPtr(), old_size, new_size));
}

void ExtensionsGuestViewContainer::CallElementResizeCallback(
    const gfx::Size& old_size,
    const gfx::Size& new_size) {
  v8::HandleScope handle_scope(element_resize_isolate_);
  v8::Local<v8::Function> callback = v8::Local<v8::Function>::New(
      element_resize_isolate_, element_resize_callback_);
  v8::Local<v8::Context> context = callback->CreationContext();
  if (context.IsEmpty())
    return;

  const int argc = 4;
  v8::Local<v8::Value> argv[argc] = {
      v8::Integer::New(element_resize_isolate_, old_size.width()),
      v8::Integer::New(element_resize_isolate_, old_size.height()),
      v8::Integer::New(element_resize_isolate_, new_size.width()),
      v8::Integer::New(element_resize_isolate_, new_size.height())};

  v8::Context::Scope context_scope(context);
  blink::WebScopedMicrotaskSuppression suppression;

  callback->Call(context->Global(), argc, argv);
}

}  // namespace extensions
