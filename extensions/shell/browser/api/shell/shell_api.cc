// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/shell/shell_api.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/shell/browser/shell_app_window.h"
#include "extensions/shell/browser/shell_desktop_controller.h"
#include "extensions/shell/common/api/shell.h"

using base::DictionaryValue;

namespace CreateWindow = extensions::shell_api::shell::CreateWindow;

namespace extensions {

namespace {

const char kInvalidArguments[] = "Invalid arguments";

// Creates a function call result to send to the renderer.
DictionaryValue* CreateResult(ShellAppWindow* app_window) {
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

ExtensionFunction::ResponseAction ShellCreateWindowFunction::Run() {
  scoped_ptr<CreateWindow::Params> params(CreateWindow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Convert "main.html" to "chrome-extension:/<id>/main.html".
  GURL url = extension()->GetResourceURL(params->url);
  if (!url.is_valid())
    return RespondNow(Error(kInvalidArguments));

  // The desktop keeps ownership of the window.
  ShellAppWindow* app_window =
      ShellDesktopController::instance()->CreateAppWindow(browser_context(),
                                                          extension());
  app_window->LoadURL(url);

  // Create the reply to send to the renderer.
  return RespondNow(OneArgument(CreateResult(app_window)));
}

}  // namespace extensions
