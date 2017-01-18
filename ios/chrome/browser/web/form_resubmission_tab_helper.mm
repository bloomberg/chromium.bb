// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/form_resubmission_tab_helper.h"

#import "ios/chrome/browser/ui/alert_coordinator/form_resubmission_coordinator.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"

DEFINE_WEB_STATE_USER_DATA_KEY(FormResubmissionTabHelper);

FormResubmissionTabHelper::FormResubmissionTabHelper(web::WebState* web_state)
    : web::WebStateObserver(web_state) {}

FormResubmissionTabHelper::~FormResubmissionTabHelper() = default;

void FormResubmissionTabHelper::PresentFormResubmissionDialog(
    CGPoint location,
    const base::Callback<void(bool)>& callback) {
  UIViewController* top_controller =
      top_view_controller::TopPresentedViewControllerFrom(
          [UIApplication sharedApplication].keyWindow.rootViewController);

  base::Callback<void(bool)> local_callback(callback);
  coordinator_.reset([[FormResubmissionCoordinator alloc]
      initWithBaseViewController:top_controller
                  dialogLocation:location
                        webState:web_state()
               completionHandler:^(BOOL should_continue) {
                 local_callback.Run(should_continue);
               }]);
  [coordinator_ start];
}

void FormResubmissionTabHelper::ProvisionalNavigationStarted(const GURL&) {
  coordinator_.reset();
}

void FormResubmissionTabHelper::WebStateDestroyed() {
  coordinator_.reset();
}
