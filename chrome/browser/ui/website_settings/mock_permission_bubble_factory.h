// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_FACTORY_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_FACTORY_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"

class Browser;
class PermissionBubbleView;
class MockPermissionBubbleView;

// Provides a skeleton class for both unit and browser testing when trying to
// test the bubble manager logic. Should not be used for anything that requires
// actual UI.
// See example usage in
// chrome/browser/ui/website_settings/permission_bubble_manager_unittest.cc
class MockPermissionBubbleFactory {
 public:
  // Set |is_browser_test| if in a browser test in order to interact with the
  // message loop. That shouldn't be done in unit tests.
  MockPermissionBubbleFactory(bool is_browser_test,
                              PermissionBubbleManager* manager);
  ~MockPermissionBubbleFactory();

  // Create method called by the PBM to show a bubble.
  scoped_ptr<PermissionBubbleView> Create(Browser* browser);

  void SetCanUpdateUi(bool can_update_ui);

  void ResetCounts();

  void DocumentOnLoadCompletedInMainFrame();

  void set_response_type(PermissionBubbleManager::AutoResponseType type) {
    response_type_ = type;
  }

  // If the current view is visible.
  bool is_visible();
  // Number of times |Show| was called on any bubble.
  int show_count() { return show_count_; }
  // Number of requests seen by the last |Show|.
  int request_count() { return requests_count_; }
  // Number of requests seen.
  int total_request_count() { return total_requests_count_; }

 private:
  friend class MockPermissionBubbleView;

  // This shouldn't be called. Is here to fail tests that try to create a bubble
  // after the factory has been destroyed.
  static scoped_ptr<PermissionBubbleView> DoNotCreate(Browser* browser);

  void UpdateResponseType();
  void ShowView(MockPermissionBubbleView* view);
  void HideView(MockPermissionBubbleView* view);

  bool is_browser_test_;
  bool can_update_ui_;
  int show_count_;
  int requests_count_;
  int total_requests_count_;
  std::vector<MockPermissionBubbleView*> views_;
  PermissionBubbleManager::AutoResponseType response_type_;

  // The bubble manager that will be associated with this factory.
  PermissionBubbleManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(MockPermissionBubbleFactory);
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_MOCK_PERMISSION_BUBBLE_FACTORY_H_
