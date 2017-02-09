// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CRIWV_WEB_MAIN_PARTS_H_
#define IOS_WEB_VIEW_INTERNAL_CRIWV_WEB_MAIN_PARTS_H_

#include "ios/web/public/app/web_main_parts.h"

#include <memory>

@protocol CRIWVDelegate;

namespace ios_web_view {
class CRIWVBrowserState;

// CRIWV-specific implementation of WebMainParts.
class CRIWVWebMainParts : public web::WebMainParts {
 public:
  explicit CRIWVWebMainParts(id<CRIWVDelegate> delegate);
  ~CRIWVWebMainParts() override;

  // WebMainParts implementation.
  void PreMainMessageLoopRun() override;

  // Returns the CRIWVBrowserState for this embedder.
  CRIWVBrowserState* browser_state() const { return browser_state_.get(); }

  // Returns the off the record CRIWVBrowserState for this embedder.
  CRIWVBrowserState* off_the_record_browser_state() const {
    return off_the_record_browser_state_.get();
  }

 private:
  // This object's delegate.
  id<CRIWVDelegate> delegate_;

  // The BrowserState for this embedder.
  std::unique_ptr<CRIWVBrowserState> browser_state_;

  // The BrowserState for this embedder.
  std::unique_ptr<CRIWVBrowserState> off_the_record_browser_state_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_CRIWV_WEB_MAIN_PARTS_H_
