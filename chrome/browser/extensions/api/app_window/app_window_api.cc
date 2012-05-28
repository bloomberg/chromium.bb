// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/app_window/app_window_api.h"

#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_window_controller.h"
#include "chrome/browser/extensions/extension_window_list.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/extensions/api/app_window.h"
#include "content/public/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/render_view_host.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"

namespace Create = extensions::api::app_window::Create;

namespace extensions {

const char kCustomFrameOption[] = "custom";

bool AppWindowCreateFunction::RunImpl() {
  scoped_ptr<Create::Params> params(Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url = GetExtension()->GetResourceURL(params->url);

  bool use_custom_frame = params->options.frame.get() &&
      *params->options.frame.get() == kCustomFrameOption;

  // TODO(jeremya): figure out a way to pass the opening WebContents through to
  // ShellWindow::Create so we can set the opener at create time rather than
  // with a hack in AppWindowCustomBindings::GetView().
  ShellWindow::CreateParams create_params;
  if (params->options.width.get())
    create_params.bounds.set_width(*params->options.width.get());
  if (params->options.height.get())
    create_params.bounds.set_height(*params->options.height.get());
  if (params->options.left.get())
    create_params.bounds.set_x(*params->options.left.get());
  if (params->options.top.get())
    create_params.bounds.set_y(*params->options.top.get());
  create_params.frame = use_custom_frame ?
      ShellWindow::CreateParams::FRAME_CUSTOM :
      ShellWindow::CreateParams::FRAME_CHROME;
  ShellWindow* shell_window =
      ShellWindow::Create(profile(), GetExtension(), url, create_params);
  shell_window->Show();

  content::WebContents* created_contents = shell_window->web_contents();
  int view_id = created_contents->GetRenderViewHost()->GetRoutingID();

  result_.reset(base::Value::CreateIntegerValue(view_id));
  return true;
}

}  // namespace extensions
