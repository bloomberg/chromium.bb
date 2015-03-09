// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_BLOCKED_POPUP_INFO_H_
#define IOS_WEB_WEB_STATE_BLOCKED_POPUP_INFO_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/web/public/referrer.h"
#include "url/gurl.h"

namespace web {

// Contain all information related to a blocked popup.
// TODO(eugenebut): rename to BlockedPopup as it's not an info object anymore.
class BlockedPopupInfo {
 public:
  BlockedPopupInfo(const GURL& url,
                   const Referrer& referrer,
                   NSString* window_name,
                   ProceduralBlock show_popup_handler);
  ~BlockedPopupInfo();

  // Returns the URL of the popup that was blocked.
  const GURL& url() const { return url_; }
  // Returns the Referrer of the URL that was blocked.
  const Referrer& referrer() const { return referrer_; }
  // Returns the window name of the popup that was blocked.
  NSString* window_name() const { return window_name_; }
  // Allows the popup by opening the blocked popup window.
  void ShowPopup() const;

  BlockedPopupInfo(const BlockedPopupInfo& blocked_popup_info);
  void operator=(const BlockedPopupInfo& blocked_popup_info);
 private:
  GURL url_;
  Referrer referrer_;
  base::scoped_nsobject<NSString> window_name_;
  ProceduralBlock show_popup_handler_;
};

}  // namespace web

#endif // IOS_WEB_WEB_STATE_BLOCKED_POPUP_INFO_H_
