// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/api/shell/shell_api.h"

#include "apps/shell/browser/shell_app_window.h"
#include "apps/shell/browser/shell_desktop_controller.h"
#include "base/values.h"

using base::DictionaryValue;

namespace apps {
namespace {

// Creates a function call result to send to the renderer.
DictionaryValue* CreateResult(apps::ShellAppWindow* app_window) {
  int view_id = app_window->GetRenderViewRoutingID();

  DictionaryValue* result = new DictionaryValue;
  result->Set("viewId", new base::FundamentalValue(view_id));
  return result;
}

}  // namespace

ShellCreateWindowFunction::ShellCreateWindowFunction() {
}

ShellCreateWindowFunction::~ShellCreateWindowFunction() {
}

bool ShellCreateWindowFunction::RunImpl() {
  // Arguments must contain a URL and may contain options and a callback.
  if (args_->GetSize() < 1 || args_->GetSize() > 3)
    return false;

  // Extract the URL for the window contents, e.g. "main.html".
  std::string url_string;
  if (!args_->GetString(0, &url_string))
    return false;

  // Convert "main.html" to "chrome-extension:/<id>/main.html".
  GURL url = GetExtension()->GetResourceURL(url_string);
  if (!url.is_valid())
    return false;

  // The desktop keeps ownership of the window.
  apps::ShellAppWindow* app_window =
      apps::ShellDesktopController::instance()->CreateAppWindow(
          browser_context());
  app_window->LoadURL(url);

  // Create the reply to send to the renderer.
  DictionaryValue* result = CreateResult(app_window);
  SetResult(result);

  SendResponse(true /* success */);
  return true;
}

}  // namespace apps
