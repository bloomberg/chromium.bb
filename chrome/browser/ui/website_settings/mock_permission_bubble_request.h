// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_REQUEST_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_REQUEST_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPermissionBubbleRequest : public PermissionBubbleRequest {
 public:
  explicit MockPermissionBubbleRequest(const std::string& text);
  virtual ~MockPermissionBubbleRequest();

  MOCK_CONST_METHOD0(GetMessageText, base::string16());
  MOCK_CONST_METHOD0(GetMessageTextFragment, base::string16());
  MOCK_CONST_METHOD0(GetAlternateAcceptButtonText, base::string16());
  MOCK_CONST_METHOD0(GetAlternateDenyButtonText, base::string16());
  MOCK_METHOD0(PermissionGranted, void());
  MOCK_METHOD0(PermissionDenied, void());
  MOCK_METHOD0(Cancelled, void());
  MOCK_METHOD0(RequestFinished, void());

  void SetText(const base::string16& text);
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_REQUEST_H_
