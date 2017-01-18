// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_FORM_RESUBMISSION_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_FORM_RESUBMISSION_TAB_HELPER_H_

#include <CoreGraphics/CoreGraphics.h>

#include "base/callback.h"
#include "base/macros.h"
#import "base/mac/scoped_nsobject.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@class FormResubmissionCoordinator;

// Allows presenting form resubmission dialog. Listens to web::WebState activity
// and dismisses the dialog when necessary.
class FormResubmissionTabHelper
    : public web::WebStateUserData<FormResubmissionTabHelper>,
      public web::WebStateObserver {
 public:
  explicit FormResubmissionTabHelper(web::WebState* web_state);
  ~FormResubmissionTabHelper() override;

  // Presents Form Resubmission Dialog at the given |location|. |callback| is
  // called with true if resubmission was confirmed and with false if it was
  // cancelled.
  void PresentFormResubmissionDialog(
      CGPoint location,
      const base::Callback<void(bool)>& callback);

 private:
  // web::WebStateObserver overrides:
  void ProvisionalNavigationStarted(const GURL&) override;
  void WebStateDestroyed() override;

  // Coordinates Form Resubmission dialog presentation.
  base::scoped_nsobject<FormResubmissionCoordinator> coordinator_;

  DISALLOW_COPY_AND_ASSIGN(FormResubmissionTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_FORM_RESUBMISSION_TAB_HELPER_H_
