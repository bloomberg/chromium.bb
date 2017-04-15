// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_PARTS_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_PARTS_H_

#include <memory>

#include "ios/web/public/app/web_main_parts.h"

@protocol CWVDelegate;

namespace ios_web_view {

// WebView implementation of WebMainParts.
class WebViewWebMainParts : public web::WebMainParts {
 public:
  WebViewWebMainParts();
  ~WebViewWebMainParts() override;

  // WebMainParts implementation.
  void PreMainMessageLoopRun() override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_PARTS_H_
