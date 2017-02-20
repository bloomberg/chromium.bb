// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_queue_controller.h"

#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"


// PermissionQueueControllerTests ---------------------------------------------

class PermissionQueueControllerTests : public ChromeRenderViewHostTestHarness {
 protected:
  PermissionQueueControllerTests() {}
  ~PermissionQueueControllerTests() override {}

  PermissionRequestID RequestID(int bridge_id) {
    return PermissionRequestID(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(),
        bridge_id);
  }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InfoBarService::CreateForWebContents(web_contents());
  }

  DISALLOW_COPY_AND_ASSIGN(PermissionQueueControllerTests);
};


// ObservationCountingQueueController -----------------------------------------

class ObservationCountingQueueController : public PermissionQueueController {
 public:
  explicit ObservationCountingQueueController(Profile* profile);
  ~ObservationCountingQueueController() override;

  int call_count() const { return call_count_; }

 private:
  int call_count_;

  // PermissionQueueController:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  DISALLOW_COPY_AND_ASSIGN(ObservationCountingQueueController);
};

ObservationCountingQueueController::ObservationCountingQueueController(
    Profile* profile)
    : PermissionQueueController(profile,
                                CONTENT_SETTINGS_TYPE_GEOLOCATION),
      call_count_(0) {}

ObservationCountingQueueController::~ObservationCountingQueueController() {
}

void ObservationCountingQueueController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED, type);
  ++call_count_;
  PermissionQueueController::Observe(type, source, details);
}


// Actual tests ---------------------------------------------------------------

TEST_F(PermissionQueueControllerTests, OneObservationPerInfoBarCancelled) {
  // When an infobar is cancelled, the infobar helper sends a notification to
  // the controller. If the controller has another infobar queued, it should
  // maintain its registration for notifications with the helper, but on the
  // last infobar cancellation it should unregister for notifications.
  //
  // What we don't want is for the controller to unregister and then re-register
  // for notifications, which can lead to getting notified multiple times.  This
  // test checks that in the case where the controller should remain registered
  // for notifications, it gets notified exactly once."
  ObservationCountingQueueController queue_controller(profile());
  GURL url("http://www.example.com/geolocation");
  base::Callback<void(ContentSetting)> callback;
  queue_controller.CreateInfoBarRequest(RequestID(0), url, url,
                                        false /* user_gesture */, callback);
  queue_controller.CreateInfoBarRequest(RequestID(1), url, url,
                                        false /* user_gesture */, callback);
  queue_controller.CancelInfoBarRequest(RequestID(0));
  EXPECT_EQ(1, queue_controller.call_count());
}

TEST_F(PermissionQueueControllerTests, FailOnBadPattern) {
  ObservationCountingQueueController queue_controller(profile());
  GURL url("chrome://settings");
  base::Callback<void(ContentSetting)> callback;
  queue_controller.CreateInfoBarRequest(RequestID(0), url, url,
                                        false /* user_gesture */, callback);
  queue_controller.CancelInfoBarRequest(RequestID(0));
  EXPECT_EQ(0, queue_controller.call_count());
}
