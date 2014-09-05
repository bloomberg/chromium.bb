// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_APP_WINDOW_APP_WINDOW_API_H_
#define EXTENSIONS_BROWSER_API_APP_WINDOW_APP_WINDOW_API_H_

#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace core_api {
namespace app_window {
struct CreateWindowOptions;
}
}

class AppWindowCreateFunction : public AsyncExtensionFunction {
 public:
  AppWindowCreateFunction();
  DECLARE_EXTENSION_FUNCTION("app.window.create", APP_WINDOW_CREATE)

 protected:
  virtual ~AppWindowCreateFunction() {}
  virtual bool RunAsync() OVERRIDE;

 private:
  bool GetBoundsSpec(
      const extensions::core_api::app_window::CreateWindowOptions& options,
      AppWindow::CreateParams* params,
      std::string* error);

  AppWindow::Frame GetFrameFromString(const std::string& frame_string);
  bool GetFrameOptions(
      const extensions::core_api::app_window::CreateWindowOptions& options,
      AppWindow::CreateParams* create_params);
  void UpdateFrameOptionsForChannel(AppWindow::CreateParams* create_params);

  bool inject_html_titlebar_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_APP_WINDOW_APP_WINDOW_API_H_
