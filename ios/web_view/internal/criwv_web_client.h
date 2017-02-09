// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CRIWV_WEB_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_CRIWV_WEB_CLIENT_H_

#include <memory>
#include "base/compiler_specific.h"
#import "ios/web/public/web_client.h"

@protocol CRIWVDelegate;

namespace ios_web_view {
class CRIWVBrowserState;
class CRIWVWebMainParts;

// CRIWV-specific implementation of WebClient.  Delegates some functionality to
// CRIWVDelegate.
class CRIWVWebClient : public web::WebClient {
 public:
  explicit CRIWVWebClient(id<CRIWVDelegate> delegate);
  ~CRIWVWebClient() override;

  // WebClient implementation.
  web::WebMainParts* CreateWebMainParts() override;
  std::string GetProduct() const override;
  std::string GetUserAgent(bool desktop_user_agent) const override;

  // Normal browser state associated with the receiver.
  CRIWVBrowserState* browser_state() const;
  // Off the record browser state  associated with the receiver.
  CRIWVBrowserState* off_the_record_browser_state() const;

 private:
  // This object's delegate.
  id<CRIWVDelegate> delegate_;

  // The WebMainParts created by |CreateWebMainParts()|.
  CRIWVWebMainParts* web_main_parts_;

  DISALLOW_COPY_AND_ASSIGN(CRIWVWebClient);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_CRIWV_WEB_CLIENT_H_
