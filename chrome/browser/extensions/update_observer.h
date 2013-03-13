// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATE_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATE_OBSERVER_H_

#include <string>

namespace extensions {

class UpdateObserver {
 public:
  // Invoked when an app update is available.
  virtual void OnAppUpdateAvailable(const std::string& app_id) = 0;

  // Invoked when Chrome update is available.
  virtual void OnChromeUpdateAvailable() = 0;

 protected:
  virtual ~UpdateObserver() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATE_OBSERVER_H_
