// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_
#define EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_

#include <string>

namespace content {
class RenderFrameHost;
}

namespace extensions {
class Extension;
class ExtensionHost;

class ProcessManagerObserver {
 public:
  // Called immediately after an extension background host is started.
  virtual void OnBackgroundHostStartup(const Extension* extension) {}

  // Called immediately after an ExtensionHost for an extension with a lazy
  // background page is created.
  virtual void OnBackgroundHostCreated(ExtensionHost* host) {}

  // Called immediately after the extension background host is destroyed.
  virtual void OnBackgroundHostClose(const std::string& extension_id) {}

  virtual void OnExtensionFrameRegistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) {}

  virtual void OnExtensionFrameUnregistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_
