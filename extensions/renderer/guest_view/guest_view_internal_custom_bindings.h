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
  // AttachGuest takes four parameters:
  // |element_instance_id| uniquely identifies a container within the content
  // module is able to host GuestViews.
  // |guest_instance_id| uniquely identifies an unattached GuestView.
  // |attach_params| is typically used to convey the current state of the
  // container element at the time of attachment. These parameters are passed
  // down to the GuestView. The GuestView may use these parameters to update the
  // state of the guest hosted in another process.
  // |callback| is an optional callback that is called once attachment is
  // complete. The callback takes in a parameter for the WindowProxy of the
  // guest identified by |guest_instance_id|.
  void AttachGuest(const v8::FunctionCallbackInfo<v8::Value>& args);

  // DetachGuest detaches the container container specified from the associated
  // GuestViewBase. DetachGuest takes two parameters:
  // |element_instance_id| uniquely identifies a container within the content
  // module is able to host GuestViews.
  // |callback| is an optional callback that is called once the container has
  // been detached.
  void DetachGuest(const v8::FunctionCallbackInfo<v8::Value>& args);

  // GetContentWindow takes in a RenderView routing ID and returns the
  // Window JavaScript object for that RenderView.
  void GetContentWindow(const v8::FunctionCallbackInfo<v8::Value>& args);

  // RegisterDestructionCallback registers a JavaScript callback function to be
  // called when the guestview's container is destroyed.
  // RegisterDestructionCallback takes in a single paramater, |callback|.
  void RegisterDestructionCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // RegisterElementResizeCallback registers a JavaScript callback function to
  // be called when the element is resized. RegisterElementResizeCallback takes
  // a single parameter, |callback|.
  void RegisterElementResizeCallback(
      const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_GUEST_VIEW_INTERNAL_CUSTOM_BINDINGS_H_
