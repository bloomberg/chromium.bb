// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace shell {

struct BubbleConfig {
  base::string16 label;
  views::View* anchor_view;
  views::BubbleBorder::Arrow arrow;
};

class ExampleBubbleDelegateView : public views::BubbleDelegateView {
 public:
  ExampleBubbleDelegateView(const BubbleConfig& config)
      : BubbleDelegateView(config.anchor_view, config.arrow),
        label_(config.label) {}

  virtual void Init() OVERRIDE {
    SetLayoutManager(new views::FillLayout());
    views::Label* label = new views::Label(label_);
    AddChildView(label);
  }

 private:
  base::string16 label_;
};

void CreatePointyBubble(views::View* anchor_view) {
  BubbleConfig config;
  config.label = base::ASCIIToUTF16("PointyBubble");
  config.anchor_view = anchor_view;
  config.arrow = views::BubbleBorder::TOP_LEFT;
  ExampleBubbleDelegateView* bubble = new ExampleBubbleDelegateView(config);
  views::BubbleDelegateView::CreateBubble(bubble)->Show();
}

}  // namespace shell
}  // namespace ash
