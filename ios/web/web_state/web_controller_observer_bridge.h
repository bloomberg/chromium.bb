// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_CONTROLLER_OBSERVER_BRIDGE_H_
#define IOS_WEB_WEB_STATE_WEB_CONTROLLER_OBSERVER_BRIDGE_H_

#import "base/ios/weak_nsobject.h"
#include "base/macros.h"
#include "ios/web/public/web_state/web_state_observer.h"

@class CRWWebController;
@protocol CRWWebControllerObserver;

namespace web {

class WebState;

// This class allows to register a CRWWebControllerObserver instance as
// a WebStateObserver.
// It listens to WebStateObserver calls and forwards them to the underlying
// CRWWebControllerObserver.
// TODO(droger): Remove this class once all CRWWebControllerObserver have been
// converted to WebStateObserver and WebState::ScriptCommandCallback.
class WebControllerObserverBridge : public WebStateObserver {
 public:
  // |web_controller_observer| and |web_controller| must not be nil, and must
  // outlive this WebControllerObserverBridge.
  WebControllerObserverBridge(
      id<CRWWebControllerObserver> web_controller_observer,
      WebState* web_state,
      CRWWebController* web_controller);

  ~WebControllerObserverBridge() override;

  // Gets the underlying CRWWebControllerObserver.
  id<CRWWebControllerObserver> web_controller_observer() const {
    return web_controller_observer_;
  }

 private:
  // WebStateObserver implementation.
  void PageLoaded(PageLoadCompletionStatus load_completion_status) override;

  base::WeakNSProtocol<id<CRWWebControllerObserver>> web_controller_observer_;
  base::WeakNSObject<CRWWebController> web_controller_;

  DISALLOW_COPY_AND_ASSIGN(WebControllerObserverBridge);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_CONTROLLER_OBSERVER_BRIDGE_H_
