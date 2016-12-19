// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_PROTOCOL_H_
#define IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_PROTOCOL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class ShareToData;

namespace ShareTo {

// Provides the result of a sharing event.
enum ShareResult {
  // The share was successful.
  SHARE_SUCCESS,
  // The share was cancelled by either the user or by the service.
  SHARE_CANCEL,
  // The share was attempted, but failed due to a network-related error.
  SHARE_NETWORK_FAILURE,
  // The share was attempted, but failed because of user authentication error.
  SHARE_SIGN_IN_FAILURE,
  // The share was attempted, but failed with an unspecified error.
  SHARE_ERROR,
  // The share was attempted, and the result is unknown.
  SHARE_UNKNOWN_RESULT,
};

}  // namespace ShareTo

// This protocol provides callbacks for sharing events.
@protocol ShareToDelegate<NSObject>
// Callback triggered on completion of sharing. |successMessage| gives the
// message to be displayed on successful completion. If |successMessage| is nil,
// no message is displayed.
- (void)shareDidComplete:(ShareTo::ShareResult)shareStatus
          successMessage:(NSString*)message;

// Callback triggered if user invoked a Password Management App Extension.
// If |shareStatus| is a successful status, delegate implementing this method
// should find a login form on the current page and autofills it with the
// |username| and |password|. |successMessage|, if non-nil, is the message to
// be displayed on successful completion.
- (void)passwordAppExDidFinish:(ShareTo::ShareResult)shareStatus
                      username:(NSString*)username
                      password:(NSString*)password
                successMessage:(NSString*)message;
@end

namespace ios {
class ChromeBrowserState;
}

@protocol ShareProtocol<NSObject>

// Returns YES if a share is currently in progress.
- (BOOL)isActive;

// Cancels share operation.
- (void)cancelShareAnimated:(BOOL)animated;

// Shares the given data. The controller should be the current controller and
// will be used to layer the necessary UI on top of it. The delegate will
// receive callbacks about sharing events. On iPad, |rect| specifies the anchor
// location for the UIPopover relative to |inView|.
// |-isEnabled| must be YES. |data|, |controller|, |delegate| must not be nil.
// When sharing to iPad, |inView| must not be nil and |rect|'s size must not be
// zero.
- (void)shareWithData:(ShareToData*)data
           controller:(UIViewController*)controller
         browserState:(ios::ChromeBrowserState*)browserState
      shareToDelegate:(id<ShareToDelegate>)delegate
             fromRect:(CGRect)rect
               inView:(UIView*)inView;
@end

#endif  // IOS_CHROME_BROWSER_UI_ACTIVITY_SERVICES_SHARE_PROTOCOL_H_
