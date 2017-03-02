// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_BLOCKED_POPUP_INFO_H_
#define IOS_WEB_WEB_STATE_BLOCKED_POPUP_INFO_H_

#include "ios/web/public/referrer.h"
#include "url/gurl.h"

namespace web {

// Contain all information related to a blocked popup.
// TODO(crbug.com/692117): Fold this class into BlockedPopupTabHelper.
class BlockedPopupInfo {
 public:
  BlockedPopupInfo(const GURL& url, const Referrer& referrer);
  ~BlockedPopupInfo();

  // Returns the URL of the popup that was blocked.
  const GURL& url() const { return url_; }
  // Returns the Referrer of the URL that was blocked.
  const Referrer& referrer() const { return referrer_; }

  BlockedPopupInfo(const BlockedPopupInfo& blocked_popup_info);
  void operator=(const BlockedPopupInfo& blocked_popup_info);
 private:
  GURL url_;
  Referrer referrer_;
};

}  // namespace web

#endif // IOS_WEB_WEB_STATE_BLOCKED_POPUP_INFO_H_
