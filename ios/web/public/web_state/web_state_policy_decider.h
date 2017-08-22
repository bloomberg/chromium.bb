// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_POLICY_DECIDER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_POLICY_DECIDER_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#include "ui/base/page_transition_types.h"

namespace web {

class WebState;

// Decides the navigation policy for a web state.
class WebStatePolicyDecider {
 public:
  // Removes self as a policy decider of |web_state_|.
  virtual ~WebStatePolicyDecider();

  // Asks the decider whether the navigation corresponding to |request| should
  // be allowed to continue. Defaults to true if not overriden.
  virtual bool ShouldAllowRequest(NSURLRequest* request,
                                  ui::PageTransition transition);

  // Asks the decider whether the navigation corresponding to |response| should
  // be allowed to continue. Defaults to true if not overriden.
  virtual bool ShouldAllowResponse(NSURLResponse* response);

  // Notifies the policy decider that the web state is being destroyed.
  // Gives subclasses a chance to cleanup.
  // The policy decider must not be destroyed while in this call, as removing
  // while iterating is not supported.
  virtual void WebStateDestroyed() {}

  WebState* web_state() const { return web_state_; }

 protected:
  // Designated constructor. Subscribes to |web_state|.
  explicit WebStatePolicyDecider(WebState* web_state);

 private:
  friend class WebStateImpl;

  // Resets the current web state.
  void ResetWebState();

  // The web state to decide navigation policy for.
  WebState* web_state_;

  DISALLOW_COPY_AND_ASSIGN(WebStatePolicyDecider);
};
}

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_POLICY_DECIDER_H_
