// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_PUBLIC_ACTIVITY_FACTORY_H_
#define ATHENA_ACTIVITY_PUBLIC_ACTIVITY_FACTORY_H_

#include "athena/athena_export.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class AppWindow;
class ShellAppWindow;
}

namespace athena {
class Activity;

class ATHENA_EXPORT ActivityFactory {
 public:
  // Registers the singleton factory.
  static void RegisterActivityFactory(ActivityFactory* factory);

  // Gets the registered singleton factory.
  static ActivityFactory* Get();

  // Shutdowns the factory.
  static void Shutdown();

  virtual ~ActivityFactory() {}

  // Create an activity of a web page. If |title| is empty, the title will be
  // obtained from the web contents.
  virtual Activity* CreateWebActivity(content::BrowserContext* browser_context,
                                      const base::string16& title,
                                      const GURL& url) = 0;

  // Create an activity of an app with |app_window| for app shell environemnt.
  // The returned activity should own |app_window|.
  // TODO(oshima): Consolidate these two methods to create AppActivity
  // once crbug.com/403726 is finished.
  virtual Activity* CreateAppActivity(extensions::ShellAppWindow* app_window,
                                      const std::string& id) = 0;

  // Create an activity of an app with |app_window| for chrome environment.
  virtual Activity* CreateAppActivity(extensions::AppWindow* app_window) = 0;
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_PUBLIC_ACTIVITY_FACTORY_H_
