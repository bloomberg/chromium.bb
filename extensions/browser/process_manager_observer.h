// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_
#define EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_

namespace extensions {
class Extension;

class ProcessManagerObserver {
 public:
  virtual ~ProcessManagerObserver() {}

  // Called immediately after an extension background host is started.
  virtual void OnBackgroundHostStartup(const Extension* extension) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_PROCESS_MANAGER_OBSERVER_H_
