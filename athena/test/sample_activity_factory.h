// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_SAMPLE_ACTIVITY_FACTORY_H_
#define ATHENA_TEST_SAMPLE_ACTIVITY_FACTORY_H_

#include "athena/activity/public/activity_factory.h"
#include "base/macros.h"

namespace athena {
namespace test {

class SampleActivityFactory : public ActivityFactory {
 public:
  SampleActivityFactory();
  virtual ~SampleActivityFactory();

  // Overridden from ActivityFactory:
  virtual Activity* CreateWebActivity(content::BrowserContext* browser_context,
                                      const base::string16& title,
                                      const GURL& url) OVERRIDE;
  virtual Activity* CreateAppActivity(extensions::AppWindow* app_window,
                                      views::WebView* web_view) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SampleActivityFactory);
};

}  // namespace test
}  // namespace athena

#endif  // ATHENA_TEST_SAMPLE_ACTIVITY_FACTORY_H_
