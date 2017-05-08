// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

#include "base/macros.h"
#include "base/timer/elapsed_timer.h"

@protocol SadTabTabHelperDelegate;

// SadTabTabHelper listens to RenderProcessGone events and presents a
// SadTabView view appropriately.
class SadTabTabHelper : public web::WebStateUserData<SadTabTabHelper>,
                        public web::WebStateObserver {
 public:
  // Creates a SadTabTabHelper and attaches it to a specific web_state object,
  // a delegate object controls presentation. Uses the default
  // |repeat_failure_interval|.
  static void CreateForWebState(web::WebState* web_state,
                                id<SadTabTabHelperDelegate> delegate);

  // Creates a SadTabTabHelper and attaches it to a specific web)state object,
  // a delegate object controls presentation, |repeat_failure_interval| sets
  // the corresponding instance variable used for determining repeat failures.
  static void CreateForWebState(web::WebState* web_state,
                                id<SadTabTabHelperDelegate> delegate,
                                double repeat_failure_interval);

  ~SadTabTabHelper() override;

 private:
  // Constructs a SadTabTabHelper, assigning the helper to a web_state and
  // providing a delegate. A default repeat_failure_interval will be used.
  SadTabTabHelper(web::WebState* web_state,
                  id<SadTabTabHelperDelegate> delegate);

  // Constructs a SadTabTabHelper allowing an optional |repeat_failure_interval|
  // value to be passed in, representing a timeout period in seconds during
  // which a second failure will be considered a 'repeated' crash rather than an
  // initial event.
  SadTabTabHelper(web::WebState* web_state,
                  id<SadTabTabHelperDelegate> delegate,
                  double repeat_failure_interval);

  // Presents a new SadTabView via the web_state object.
  void PresentSadTab(const GURL& url_causing_failure);

  // WebStateObserver:
  void RenderProcessGone() override;

  // A TabHelperDelegate that can control aspects of this tab helper's behavior.
  __weak id<SadTabTabHelperDelegate> delegate_;

  // Stores the last URL that caused a renderer crash,
  // used to detect repeated crashes.
  GURL last_failed_url_;

  // Stores the last date that the renderer crashed,
  // used to determine time window for repeated crashes.
  std::unique_ptr<base::ElapsedTimer> last_failed_timer_;

  // Stores the interval window during which a second RenderProcessGone failure
  // will be considered a repeat failure.
  double repeat_failure_interval_;

  DISALLOW_COPY_AND_ASSIGN(SadTabTabHelper);
};
