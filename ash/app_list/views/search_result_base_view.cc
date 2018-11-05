// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_base_view.h"

namespace app_list {

SearchResultBaseView::SearchResultBaseView() : Button(this) {}

SearchResultBaseView::~SearchResultBaseView() = default;

bool SearchResultBaseView::SkipDefaultKeyEventProcessing(
    const ui::KeyEvent& event) {
  // Ensure accelerators take priority in the app list. This ensures, e.g., that
  // Ctrl+Space will switch input methods rather than activate the button.
  return false;
}

void SearchResultBaseView::SetBackgroundHighlighted(bool enabled) {
  background_highlighted_ = enabled;
  SchedulePaint();
}

}  // namespace app_list
