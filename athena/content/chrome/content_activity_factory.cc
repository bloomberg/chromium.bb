// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/content_activity_factory.h"

#include "athena/content/app_activity.h"
#include "extensions/browser/app_window/app_window.h"

// TODO(oshima): Consolidate this and app shell implementation once
// crbug.com/403726 is fixed.
namespace athena {
namespace {

class ChromeAppActivity : public AppActivity {
 public:
  explicit ChromeAppActivity(extensions::AppWindow* app_window)
      : AppActivity(app_window->extension_id()), app_window_(app_window) {}

 private:
  virtual ~ChromeAppActivity() {}

  // AppActivity:
  virtual content::WebContents* GetWebContents() OVERRIDE {
    return app_window_->web_contents();
  }

  // Not owned.
  extensions::AppWindow* app_window_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppActivity);
};

}  // namespace

Activity* ContentActivityFactory::CreateAppActivity(
    extensions::AppWindow* app_window) {
  return new ChromeAppActivity(app_window);
}

Activity* ContentActivityFactory::CreateAppActivity(
    extensions::ShellAppWindow* app_window,
    const std::string& app_id) {
  return NULL;
}

}  // namespace athena
