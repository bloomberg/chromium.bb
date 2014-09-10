// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"

namespace content {

class DevToolsManagerDelegate;

// This class is a singleton that manage global DevTools state for the whole
// browser.
class DevToolsManager {
 public:
  // Returns single instance of this class. The instance is destroyed on the
  // browser main loop exit so this method MUST NOT be called after that point.
  static DevToolsManager* GetInstance();

  DevToolsManager();
  virtual ~DevToolsManager();

  DevToolsManagerDelegate* delegate() const { return delegate_.get(); }
  void OnClientAttached();
  void OnClientDetached();

 private:
  friend struct DefaultSingletonTraits<DevToolsManager>;

  scoped_ptr<DevToolsManagerDelegate> delegate_;
  int client_count_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
