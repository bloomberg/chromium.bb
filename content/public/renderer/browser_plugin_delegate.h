// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_BROWSER_PLUGIN_DELEGATE_H_
#define CONTENT_PUBLIC_RENDERER_BROWSER_PLUGIN_DELEGATE_H_

#include <string>

#include "content/common/content_export.h"
#include "ipc/ipc_message.h"

namespace gfx {
class Size;
}

namespace v8 {
class Isolate;
class Object;
template<typename T> class Local;
}  // namespace v8

namespace content {

class RenderFrame;

// A delegate for BrowserPlugin which gets notified about the plugin load.
// Implementations can provide additional steps necessary to change the load
// behavior of the plugin.
class CONTENT_EXPORT BrowserPluginDelegate {
 public:
  virtual ~BrowserPluginDelegate() {}

  // Called when the BrowserPlugin's geometry has been computed for the first
  // time.
  virtual void Ready() {}

  // Called when plugin document has finished loading.
  virtual void DidFinishLoading() {}

  // Called when plugin document receives data.
  virtual void DidReceiveData(const char* data, int data_length) {}

  // Sets the instance ID that idenfies the plugin within current render
  // process.
  virtual void SetElementInstanceID(int element_instance_id) {}

  // Called when the plugin resizes.
  virtual void DidResizeElement(const gfx::Size& old_size,
                                const gfx::Size& new_size) {}

  // Called when a message is received.  Returns true iff the message was
  // handled.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Return a scriptable object for the plugin.
  virtual v8::Local<v8::Object> V8ScriptableObject(v8::Isolate* isolate);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_BROWSER_PLUGIN_DELEGATE_H_
