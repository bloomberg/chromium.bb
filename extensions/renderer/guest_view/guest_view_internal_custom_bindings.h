// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_INTERNAL_CUSTOM_BINDINGS_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_INTERNAL_CUSTOM_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {
class Dispatcher;

// Implements custom bindings for the guestViewInternal API.
class GuestViewInternalCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit GuestViewInternalCustomBindings(ScriptContext* context);

 private:
  // AttachGuest attaches a GuestView to a provided container element. Once
  // attached, the GuestView will participate in layout of the container page
  // and become visible on screen.
  // AttachGuest takes three parameters:
  // |element_instance_id| uniquely identifies a container within the content
  // module is able to host GuestViews.
  // |guest_instance_id| uniquely identifies an unattached GuestView.
  // |attach_params| is typically used to convey the current state of the
  // container element at the time of attachment. These parameters are passed
  // down to the GuestView. The GuestView may use these parameters to update the
  // state of the guest hosted in another process.
  void AttachGuest(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_INTERNAL_CUSTOM_BINDINGS_H_
