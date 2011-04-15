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
  RemoveAllInfoBarsForDestruction();
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
    InfoBarView* child = static_cast<InfoBarView*>(GetChildViewAt(i));
    top -= child->arrow_height();
    int child_height = child->total_height();
    child->SetBounds(0, top, width(), child_height);
    top += child_height;
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
