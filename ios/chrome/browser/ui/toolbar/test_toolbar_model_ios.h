// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_MODEL_IOS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_MODEL_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "components/toolbar/test_toolbar_model.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"

class TestToolbarModelIOS : public ToolbarModelIOS {
 public:
  TestToolbarModelIOS();
  ~TestToolbarModelIOS() override;

  // Getter to the TestToolbarModel to set its values in tests.
  TestToolbarModel* GetToolbarModel();

  // ToolbarModelIOS implementation:
  bool IsLoading() override;
  CGFloat GetLoadProgressFraction() override;
  bool CanGoBack() override;
  bool CanGoForward() override;
  bool IsCurrentTabNativePage() override;
  bool IsCurrentTabBookmarked() override;
  bool IsCurrentTabBookmarkedByUser() override;
  bool ShouldDisplayHintText() override;

  base::string16 GetFormattedURL(size_t* prefix_end) const override;
  GURL GetURL() const override;
  security_state::SecurityLevel GetSecurityLevel(
      bool ignore_editing) const override;
  gfx::VectorIconId GetVectorIcon() const override;
  base::string16 GetSecureVerboseText() const override;
  base::string16 GetEVCertName() const override;
  bool ShouldDisplayURL() const override;

  void set_is_loading(bool is_loading) { is_loading_ = is_loading; }
  void set_load_progress_fraction(CGFloat load_progress_fraction) {
    load_progress_fraction_ = load_progress_fraction;
  }
  void set_can_go_back(bool can_go_back) { can_go_back_ = can_go_back; }
  void set_can_go_forward(bool can_go_forward) {
    can_go_forward_ = can_go_forward;
  }
  void set_is_current_tab_native_page(bool is_current_tab_native_page) {
    is_current_tab_native_page_ = is_current_tab_native_page;
  }
  void set_is_current_tab_bookmarked(bool is_current_tab_bookmarked) {
    is_current_tab_bookmarked_ = is_current_tab_bookmarked;
  }

 private:
  std::unique_ptr<TestToolbarModel> test_toolbar_model_;
  bool is_loading_;
  CGFloat load_progress_fraction_;
  bool can_go_back_;
  bool can_go_forward_;
  bool is_current_tab_native_page_;
  bool is_current_tab_bookmarked_;
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_MODEL_IOS_H_
