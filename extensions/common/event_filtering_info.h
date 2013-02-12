// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EVENT_FILTERING_INFO_H_
#define EXTENSIONS_COMMON_EVENT_FILTERING_INFO_H_

#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"

namespace base {
class Value;
}

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

  std::string AsJSONString() const;
  scoped_ptr<base::Value> AsValue() const;
  bool IsEmpty() const;

 private:
  bool has_url_;
  GURL url_;

  // Allow implicit copy and assignment.
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EVENT_FILTERING_INFO_H_
