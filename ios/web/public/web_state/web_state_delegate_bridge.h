// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_BRIDGE_H_
#define IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/ios/weak_nsobject.h"
#include "ios/web/public/web_state/web_state_delegate.h"

// Objective-C interface for web::WebStateDelegate.
@protocol CRWWebStateDelegate<NSObject>
@optional

// Called when the page has made some progress loading. |progress| is a value
// between 0.0 (nothing loaded) to 1.0 (page fully loaded).
- (void)webState:(web::WebState*)webState didChangeProgress:(double)progress;

// Called when the user triggers the context menu with the given
// |ContextMenuParams|. Returns YES if the context menu operation was
// handled by the delegate. If this method is not implemented, the system
// context menu will be displayed.
- (BOOL)webState:(web::WebState*)webState
    handleContextMenu:(const web::ContextMenuParams&)params;

@end

namespace web {

// Adapter to use an id<CRWWebStateDelegate> as a web::WebStateDelegate.
class WebStateDelegateBridge : public web::WebStateDelegate {
 public:
  explicit WebStateDelegateBridge(id<CRWWebStateDelegate> delegate);
  ~WebStateDelegateBridge() override;

  // web::WebStateDelegate methods.
  void LoadProgressChanged(WebState* source, double progress) override;
  bool HandleContextMenu(WebState* source,
                         const ContextMenuParams& params) override;

 private:
  // CRWWebStateDelegate which receives forwarded calls.
  base::WeakNSProtocol<id<CRWWebStateDelegate>> delegate_;
  DISALLOW_COPY_AND_ASSIGN(WebStateDelegateBridge);
};

}  // web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_WEB_STATE_DELEGATE_BRIDGE_H_
