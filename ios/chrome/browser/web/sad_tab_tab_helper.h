// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@protocol SadTabTabHelperDelegate;

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// SadTabTabHelper listens to RenderProcessGone events and presents a
// SadTabView view appropriately.
class SadTabTabHelper : public web::WebStateUserData<SadTabTabHelper>,
                        public web::WebStateObserver {
 public:
  // Creates a SadTabTabHelper and attaches it to a specific web_state object,
  // a delegate object controls presentation.
  static void CreateForWebState(web::WebState* web_state,
                                id<SadTabTabHelperDelegate> delegate);

 private:
  // Constructor for SadTabTabHelper, assigning the helper to a web_state and
  // providing a delegate.
  SadTabTabHelper(web::WebState* web_state,
                  id<SadTabTabHelperDelegate> delegate);

  ~SadTabTabHelper() override;

  // Presents a new SadTabView via the web_state object.
  void PresentSadTab();

  // WebStateObserver:
  void RenderProcessGone() override;

  // A TabHelperDelegate that can control aspects of this tab helper's behavior.
  __weak id<SadTabTabHelperDelegate> delegate_;
};
