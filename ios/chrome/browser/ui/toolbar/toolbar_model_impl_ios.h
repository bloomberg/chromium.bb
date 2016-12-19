// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IMPL_IOS_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IMPL_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "ios/chrome/browser/ui/toolbar/toolbar_model_delegate_ios.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"

class ToolbarModelImplIOS : public ToolbarModelIOS {
 public:
  explicit ToolbarModelImplIOS(ToolbarModelDelegateIOS* delegate);
  ~ToolbarModelImplIOS() override;

  // ToolbarModelIOS implementation:
  ToolbarModel* GetToolbarModel() override;
  bool IsLoading() override;
  CGFloat GetLoadProgressFraction() override;
  bool CanGoBack() override;
  bool CanGoForward() override;
  bool IsCurrentTabNativePage() override;
  bool IsCurrentTabBookmarked() override;
  bool IsCurrentTabBookmarkedByUser() override;
  bool ShouldDisplayHintText() override;

 private:
  ToolbarModelDelegateIOS* delegate_;
  std::unique_ptr<ToolbarModel> toolbar_model_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarModelImplIOS);
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IMPL_IOS_H_
