// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/blocked_popup_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

BlockedPopupInfo::BlockedPopupInfo(const GURL& url, const Referrer& referrer)
    : url_(url), referrer_(referrer) {}

BlockedPopupInfo::BlockedPopupInfo(const BlockedPopupInfo& blocked_popup_info)
    : url_(blocked_popup_info.url_), referrer_(blocked_popup_info.referrer_) {}

BlockedPopupInfo::~BlockedPopupInfo() {}

void BlockedPopupInfo::operator=(const BlockedPopupInfo& blocked_popup_info) {
  url_ = blocked_popup_info.url_;
  referrer_ = blocked_popup_info.referrer_;
}

}  // namespace web
