// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_OBSERVER_BRIDGE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include <string>

#import "base/ios/weak_nsobject.h"
#import "ios/web/public/web_state/web_state_observer.h"

// Observes page lifecyle events from Objective-C. To use as a
// web::WebStateObserver, wrap in a web::WebStateObserverBridge.
// NOTE: This is far from complete. Add new methods as needed.
@protocol CRWWebStateObserver<NSObject>
@optional

// Page lifecycle methods. These are equivalent to the corresponding methods
// in web::WebStateObserver.
- (void)pageLoaded:(web::WebState*)webState;
- (void)documentSubmitted:(web::WebState*)webState
                 formName:(const std::string&)formName
          userInteraction:(BOOL)userInteraction;
- (void)formActivity:(web::WebState*)webState
            formName:(const std::string&)formName
           fieldName:(const std::string&)fieldName
                type:(const std::string&)type
               value:(const std::string&)value
             keyCode:(int)keyCode
               error:(BOOL)error;
// Note: after |webStateDestroyed:| is invoked, the WebState being observed
// is no longer valid.
- (void)webStateDestroyed:(web::WebState*)webState;

@end

namespace web {

class WebState;

// Bridge to use an id<CRWWebStateObserver> as a web::WebStateObserver.
// Will be added/removed as an observer of the underlying WebState during
// construction/destruction. Instances should be owned by instances of the
// class they're bridging.
class WebStateObserverBridge : public web::WebStateObserver {
 public:
  WebStateObserverBridge(web::WebState* web_state,
                         id<CRWWebStateObserver> observer);
  ~WebStateObserverBridge() override;

  // web::WebStateObserver:
  // NOTE: This is far from complete. Add new methods as needed.
  void PageLoaded(
      web::PageLoadCompletionStatus load_completion_status) override;
  void DocumentSubmitted(const std::string& form_name,
                         bool user_interaction) override;
  void FormActivityRegistered(const std::string& form_name,
                              const std::string& field_name,
                              const std::string& type,
                              const std::string& value,
                              int key_code,
                              bool error) override;
  void WebStateDestroyed() override;

 private:
  base::WeakNSProtocol<id<CRWWebStateObserver>> observer_;
  DISALLOW_COPY_AND_ASSIGN(WebStateObserverBridge);
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_OBSERVER_BRIDGE_H_
