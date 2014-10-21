// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/application_launch_web_app.h"

#include <string>

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_factory.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace {

const extensions::Extension* GetExtension(const AppLaunchParams& params) {
  if (params.extension_id.empty())
    return NULL;

  using extensions::ExtensionRegistry;
  ExtensionRegistry* registry = ExtensionRegistry::Get(params.profile);
  return registry->GetExtensionById(params.extension_id,
                                    ExtensionRegistry::ENABLED |
                                        ExtensionRegistry::DISABLED |
                                        ExtensionRegistry::TERMINATED);
}

}  // namespace

content::WebContents* OpenWebAppWindow(const AppLaunchParams& params,
                                       const GURL& url) {
  const extensions::Extension* extension = GetExtension(params);
  athena::Activity* activity =
      athena::ActivityFactory::Get()->CreateWebActivity(
          params.profile, base::UTF8ToUTF16(extension->name()), url);
  athena::Activity::Show(activity);

  // TODO: http://crbug.com/8123 we should not need to set the initial focus
  //       explicitly.
  content::WebContents* web_contents = activity->GetWebContents();
  web_contents->SetInitialFocus();
  return web_contents;
}

content::WebContents* OpenWebAppTab(const AppLaunchParams& params,
                                    const GURL& url) {
  // There are no tabbed windows in Athena. Open a new window instead.
  return OpenWebAppWindow(params, url);
}
