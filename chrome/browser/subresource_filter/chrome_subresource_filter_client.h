// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_

#include "base/macros.h"
#include "components/subresource_filter/core/browser/subresource_filter_client.h"

namespace content {
class WebContents;
}  // namespace content

// Chrome implementation of SubresourceFilterClient.
class ChromeSubresourceFilterClient
    : public subresource_filter::SubresourceFilterClient {
 public:
  explicit ChromeSubresourceFilterClient(content::WebContents* web_contents);
  ~ChromeSubresourceFilterClient() override;

  // SubresourceFilterClient:
  void ToggleNotificationVisibility(bool visibility) override;

 private:
  content::WebContents* web_contents_;
  bool shown_for_navigation_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSubresourceFilterClient);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_CHROME_SUBRESOURCE_FILTER_CLIENT_H_
