// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EVENT_FILTERING_INFO_H_
#define CHROME_COMMON_EXTENSIONS_EVENT_FILTERING_INFO_H_
#pragma once

#include "googleurl/src/gurl.h"

namespace extensions {

// Extra information about an event that is used in event filtering.
//
// This is the information that is matched against criteria specified in JS
// extension event listeners. Eg:
//
// chrome.someApi.onSomeEvent.addListener(cb,
//                                        {url: [{hostSuffix: 'google.com'}],
//                                         tabId: 1});
class EventFilteringInfo {
 public:
  EventFilteringInfo();
  ~EventFilteringInfo();
  void SetURL(const GURL& url);

  bool has_url() const { return has_url_; }
  const GURL& url() const { return url_; }

 private:
  bool has_url_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(EventFilteringInfo);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_EVENT_FILTERING_INFO_H_
