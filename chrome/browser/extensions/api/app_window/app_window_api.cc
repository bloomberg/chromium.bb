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

namespace Create = extensions::api::app_window::Create;

namespace extensions {

bool AppWindowCreateFunction::RunImpl() {
  scoped_ptr<Create::Params> params(Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url = GetExtension()->GetResourceURL(params->url);

  // TODO(jeremya): figure out a way to pass the opening WebContents through to
  // ShellWindow::Create so we can set the opener at create time rather than
  // with a hack in AppWindowCustomBindings::GetView().
  ShellWindow* shell_window =
      ShellWindow::Create(profile(), GetExtension(), url);
  // TODO(jeremya): allow caller to specify window bounds.

  content::WebContents* created_contents = shell_window->web_contents();
  int view_id = created_contents->GetRenderViewHost()->GetRoutingID();

  result_.reset(base::Value::CreateIntegerValue(view_id));
  return true;
}

}  // namespace extensions
