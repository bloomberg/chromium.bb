// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_NON_ACCESSIBLE_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_NON_ACCESSIBLE_IMAGE_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/image_view.h"

class NonAccessibleImageView : public views::ImageView {
 public:
  NonAccessibleImageView() {}
  ~NonAccessibleImageView() override {}

 private:
  // Overridden from views::ImageView.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  DISALLOW_COPY_AND_ASSIGN(NonAccessibleImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_NON_ACCESSIBLE_IMAGE_VIEW_H_
