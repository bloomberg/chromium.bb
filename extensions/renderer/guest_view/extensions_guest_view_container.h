// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_H_
#define EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_H_

#include <queue>

#include "extensions/renderer/guest_view/guest_view_container.h"
#include "v8/include/v8.h"

namespace gfx {
class Size;
}

namespace extensions {

class ExtensionsGuestViewContainer : public GuestViewContainer {
 public:
  explicit ExtensionsGuestViewContainer(content::RenderFrame* render_frame);
  ~ExtensionsGuestViewContainer() override;

  void RegisterDestructionCallback(v8::Local<v8::Function> callback,
                                   v8::Isolate* isolate);
  void RegisterElementResizeCallback(v8::Local<v8::Function> callback,
                                     v8::Isolate* isolate);

  // BrowserPluginDelegate implementation.
  void DidResizeElement(const gfx::Size& old_size,
                        const gfx::Size& new_size) override;

 private:
  void CallElementResizeCallback(const gfx::Size& old_size,
                                 const gfx::Size& new_size);

  v8::Global<v8::Function> destruction_callback_;
  v8::Isolate* destruction_isolate_;

  v8::Global<v8::Function> element_resize_callback_;
  v8::Isolate* element_resize_isolate_;

  // Weak pointer factory used for calling the element resize callback.
  base::WeakPtrFactory<ExtensionsGuestViewContainer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsGuestViewContainer);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_GUEST_VIEW_EXTENSIONS_GUEST_VIEW_CONTAINER_H_
