// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container_view.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "grit/generated_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"

InfoBarContainerView::InfoBarContainerView(Delegate* delegate)
    : InfoBarContainer(delegate) {
  SetID(VIEW_ID_INFO_BAR_CONTAINER);
}

InfoBarContainerView::~InfoBarContainerView() {
}

int InfoBarContainerView::VerticalOverlap() {
  return GetVerticalOverlap(NULL);
}

gfx::Size InfoBarContainerView::GetPreferredSize() {
  // We do not have a preferred width (we will expand to fit the available width
  // of the delegate).
  int total_height;
  GetVerticalOverlap(&total_height);
  return gfx::Size(0, total_height);
}

void InfoBarContainerView::Layout() {
  int top = GetVerticalOverlap(NULL);

  for (int i = 0; i < child_count(); ++i) {
    View* child = GetChildViewAt(i);
    gfx::Size ps = child->GetPreferredSize();
    top -= static_cast<InfoBarView*>(child)->AnimatedTabHeight();
    child->SetBounds(0, top, width(), ps.height());
    top += ps.height();
  }
}

void InfoBarContainerView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_INFOBAR_CONTAINER);
}

void InfoBarContainerView::PlatformSpecificAddInfoBar(InfoBar* infobar) {
  AddChildView(static_cast<InfoBarView*>(infobar));
}

void InfoBarContainerView::PlatformSpecificRemoveInfoBar(InfoBar* infobar) {
  RemoveChildView(static_cast<InfoBarView*>(infobar));
}

int InfoBarContainerView::GetVerticalOverlap(int* total_height) {
  // Our |total_height| is the sum of the preferred heights of the InfoBars
  // contained within us plus the |vertical_overlap|.
  int vertical_overlap = 0;
  int next_child_y = 0;

  for (int i = 0; i < child_count(); ++i) {
    View* child = GetChildViewAt(i);
    gfx::Size ps = child->GetPreferredSize();
    next_child_y -= static_cast<InfoBarView*>(child)->AnimatedTabHeight();
    vertical_overlap = std::max(vertical_overlap, -next_child_y);
    next_child_y += child->GetPreferredSize().height();
  }

  if (total_height)
    *total_height = next_child_y + vertical_overlap;
  return vertical_overlap;
}
