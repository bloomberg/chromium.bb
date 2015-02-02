// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_
#define EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_

#include <string>

namespace extensions {
class Extension;

class ProcessManagerObserver {
 public:
  // Called immediately after an extension background host is started.
  virtual void OnBackgroundHostStartup(const Extension* extension) {}

  // Called immediately after an ExtensionHost for an extension with a lazy
  // background page is created.
  virtual void OnBackgroundHostCreated(ExtensionHost* host) {}

  // Called immediately after the extension background host is destroyed.
  virtual void OnBackgroundHostClose(const std::string& extension_id) {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_
