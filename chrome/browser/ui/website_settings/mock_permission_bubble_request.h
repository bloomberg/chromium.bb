// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_REQUEST_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_REQUEST_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "url/gurl.h"

class MockPermissionBubbleRequest : public PermissionBubbleRequest {
 public:
  MockPermissionBubbleRequest();
  explicit MockPermissionBubbleRequest(const std::string& text);
  MockPermissionBubbleRequest(const std::string& text,
                              PermissionBubbleType bubble_type);
  MockPermissionBubbleRequest(const std::string& text, const GURL& url);
  MockPermissionBubbleRequest(const std::string& text,
                              const std::string& accept_label,
                              const std::string& deny_label);

  ~MockPermissionBubbleRequest() override;

  int GetIconId() const override;
  base::string16 GetMessageTextFragment() const override;
  GURL GetOrigin() const override;

  void PermissionGranted() override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;
  PermissionBubbleType GetPermissionBubbleType() const override;

  bool granted();
  bool cancelled();
  bool finished();

 private:
  MockPermissionBubbleRequest(const std::string& text,
                              const std::string& accept_label,
                              const std::string& deny_label,
                              const GURL& url,
                              PermissionBubbleType bubble_type);
  bool granted_;
  bool cancelled_;
  bool finished_;
  PermissionBubbleType bubble_type_;

  base::string16 text_;
  base::string16 accept_label_;
  base::string16 deny_label_;
  GURL origin_;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_REQUEST_H_
