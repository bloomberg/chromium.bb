// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_VIEW_H_

#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/browser/ui/website_settings/permission_bubble_view.h"

// Provides a skeleton class for unit testing, when trying to test the bubble
// manager logic. Should not be used for anything that requires actual UI.
// See example usage in
// chrome/browser/geolocation/geolocation_permission_context_unittest.cc.
class MockPermissionBubbleView : public PermissionBubbleView {
 public:
  MockPermissionBubbleView();
  ~MockPermissionBubbleView() override;

  // PermissionBubbleView:
  void SetDelegate(Delegate* delegate) override;
  void Show(const std::vector<PermissionBubbleRequest*>& requests,
            const std::vector<bool>& accept_state) override;
  bool CanAcceptRequestUpdate() override;
  void Hide() override;
  bool IsVisible() override;

  // Wrappers that update the state of the bubble.
  void Accept();
  void Deny();
  void Close();
  void Clear();

  // If we're in a browser test, we need to interact with the message loop.
  // But that shouldn't be done in unit tests.
  void SetBrowserTest(bool browser_test);

 private:
  bool visible_;
  bool can_accept_updates_;
  Delegate* delegate_;
  bool browser_test_;
  std::vector<PermissionBubbleRequest*> permission_requests_;
  std::vector<bool> permission_states_;
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_VIEW_H_
