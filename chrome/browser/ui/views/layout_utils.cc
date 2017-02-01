// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/layout_utils.h"

#include "chrome/browser/ui/views/harmony/layout_delegate.h"
#include "ui/views/view.h"

namespace layout_utils {

views::GridLayout* CreatePanelLayout(views::View* host) {
  views::GridLayout* layout = new views::GridLayout(host);
  LayoutDelegate* delegate = LayoutDelegate::Get();
  layout->SetInsets(gfx::Insets(
      delegate->GetMetric(LayoutDelegate::Metric::PANEL_CONTENT_MARGIN),
      delegate->GetMetric(LayoutDelegate::Metric::DIALOG_BUTTON_MARGIN)));
  host->SetLayoutManager(layout);
  return layout;
}

}  // namespace layout_utils
