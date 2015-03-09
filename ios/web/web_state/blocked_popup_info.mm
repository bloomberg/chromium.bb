// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/blocked_popup_info.h"

namespace web {

BlockedPopupInfo::BlockedPopupInfo(const GURL& url,
                                   const Referrer& referrer,
                                   NSString* window_name,
                                   ProceduralBlock show_popup_handler)
    : url_(url),
      referrer_(referrer),
      window_name_([window_name copy]),
      show_popup_handler_([show_popup_handler copy]){
}

BlockedPopupInfo::BlockedPopupInfo(const BlockedPopupInfo& blocked_popup_info)
    : url_(blocked_popup_info.url_),
      referrer_(blocked_popup_info.referrer_),
      window_name_([blocked_popup_info.window_name_ copy]),
      show_popup_handler_([blocked_popup_info.show_popup_handler_ copy]) {
}

BlockedPopupInfo::~BlockedPopupInfo() {}

void BlockedPopupInfo::ShowPopup() const {
  show_popup_handler_();
}

void BlockedPopupInfo::operator=(const BlockedPopupInfo& blocked_popup_info) {
  url_ = blocked_popup_info.url_;
  referrer_ = blocked_popup_info.referrer_;
  window_name_.reset([blocked_popup_info.window_name_ copy]);
  show_popup_handler_ = [blocked_popup_info.show_popup_handler_ copy];
}

}  // namespace web
