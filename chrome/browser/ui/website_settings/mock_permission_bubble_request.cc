// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/mock_permission_bubble_request.h"

#include "base/strings/utf_string_conversions.h"

MockPermissionBubbleRequest::MockPermissionBubbleRequest(
    const std::string& text) {
  SetText(base::UTF8ToUTF16(text));
}

MockPermissionBubbleRequest::~MockPermissionBubbleRequest() {}

void MockPermissionBubbleRequest::SetText(const base::string16& text) {
  ON_CALL(*this, GetMessageText()).WillByDefault(testing::Return(text));
  ON_CALL(*this, GetMessageTextFragment()).WillByDefault(testing::Return(text));
}
