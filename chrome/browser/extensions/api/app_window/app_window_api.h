// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_APP_WINDOW_APP_WINDOW_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_APP_WINDOW_APP_WINDOW_API_H_

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class AppWindowCreateFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.window.create", APP_WINDOW_CREATE)

  void SendDelayedResponse();

 protected:
  virtual ~AppWindowCreateFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_APP_WINDOW_APP_WINDOW_API_H_
