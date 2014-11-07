// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity.h"
#include "athena/test/base/test_util.h"
#include "athena/test/chrome/athena_chrome_browser_test.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "url/gurl.h"

namespace athena {

// Checks that the web activity helpers are created prior to the RenderView
// being created.
class HelpersCreatedChecker : public content::NotificationObserver {
 public:
  HelpersCreatedChecker() : helpers_created_prior_to_render_view_(true) {
    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
                   content::NotificationService::AllSources());
  }

  ~HelpersCreatedChecker() override {}

  // Returns true if WebActivity helpers were attached to the WebContents prior
  // to the RenderView being created for the WebContents.
  bool HelpersCreatedPriorToRenderView() const {
    return helpers_created_prior_to_render_view_;
  }

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_RENDER_VIEW_HOST_CREATED,
              type);

    // Assume that all of the WebActivity helpers were created if the
    // extensions::TabHelper was created.
    content::WebContents* web_contents =
        content::Source<content::WebContents>(source).ptr();
    helpers_created_prior_to_render_view_ &=
        (extensions::TabHelper::FromWebContents(web_contents) != nullptr);
  }

  bool helpers_created_prior_to_render_view_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(HelpersCreatedChecker);
};

typedef AthenaChromeBrowserTest WebActivityHelpersBrowserTest;

// Test that the WebActivity helpers are attached to the web contents prior to
// the RenderView view being created. This is important because some of the
// helpers do processing as a result of the RenderView being created.
// Note: Helpers may not be attached prior to the RenderView being created when
// a WebActivity is created as a result of
// WebContentsDelegate::AddNewContents(). Desktop Chrome has a similar problem.
IN_PROC_BROWSER_TEST_F(WebActivityHelpersBrowserTest, CreationTime) {
  HelpersCreatedChecker checker;
  GURL url("http://www.google.com");

  athena::Activity* activity = test_util::CreateTestWebActivity(
      GetBrowserContext(), base::UTF8ToUTF16("App"), url);
  content::WebContents* web_contents1 = activity->GetWebContents();
  EXPECT_NE(Activity::ACTIVITY_UNLOADED, activity->GetCurrentState());
  EXPECT_TRUE(web_contents1->GetRenderViewHost()->IsRenderViewLive());
  EXPECT_TRUE(checker.HelpersCreatedPriorToRenderView());

  activity->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  activity->SetCurrentState(Activity::ACTIVITY_VISIBLE);

  content::WebContents* web_contents2 = activity->GetWebContents();
  EXPECT_NE(web_contents1, web_contents2);
  EXPECT_TRUE(web_contents2->GetRenderViewHost()->IsRenderViewLive());
  EXPECT_TRUE(checker.HelpersCreatedPriorToRenderView());
}

}  // namespace athena
