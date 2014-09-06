// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_BROWSER_PLUGIN_DELEGATE_H_
#define CONTENT_PUBLIC_RENDERER_BROWSER_PLUGIN_DELEGATE_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

class RenderFrame;

// A delegate for BrowserPlugin which gets notified about the plugin load.
// Implementations can provide additional steps necessary to change the load
// behavior of the plugin.
class CONTENT_EXPORT BrowserPluginDelegate {
 public:
  BrowserPluginDelegate(RenderFrame* render_frame,
                        const std::string& mime_type) {}
  virtual ~BrowserPluginDelegate() {}

  // Called when plugin document has finished loading.
  virtual void DidFinishLoading() {}

  // Called when plugin document receives data.
  virtual void DidReceiveData(const char* data, int data_length) {}

  // Sets the instance ID that idenfies the plugin within current render
  // process.
  virtual void SetElementInstanceID(int element_instance_id) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_BROWSER_PLUGIN_DELEGATE_H_
