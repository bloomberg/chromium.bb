// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container_view.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"

// static
const char InfoBarContainerView::kViewClassName[] = "InfoBarContainerView";

InfoBarContainerView::InfoBarContainerView(Delegate* delegate)
    : infobars::InfoBarContainer(delegate) {
  set_id(VIEW_ID_INFO_BAR_CONTAINER);
}

InfoBarContainerView::~InfoBarContainerView() {
  RemoveAllInfoBarsForDestruction();
}

gfx::Size InfoBarContainerView::GetPreferredSize() const {
  int total_height;
  GetVerticalOverlap(&total_height);
  gfx::Size size(0, total_height);
  for (int i = 0; i < child_count(); ++i)
    size.SetToMax(gfx::Size(child_at(i)->GetPreferredSize().width(), 0));
  return size;
}

const char* InfoBarContainerView::GetClassName() const {
  return kViewClassName;
}

void InfoBarContainerView::Layout() {
  int top = GetVerticalOverlap(NULL);

  for (int i = 0; i < child_count(); ++i) {
    InfoBarView* child = static_cast<InfoBarView*>(child_at(i));
    top -= child->arrow_height();
    int child_height = child->total_height();
    child->SetBounds(0, top, width(), child_height);
    top += child_height;
  }
}

void InfoBarContainerView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_GROUP;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_INFOBAR_CONTAINER);
}

void InfoBarContainerView::PlatformSpecificAddInfoBar(
    infobars::InfoBar* infobar,
    size_t position) {
  AddChildViewAt(static_cast<InfoBarView*>(infobar),
                 static_cast<int>(position));
}

void InfoBarContainerView::PlatformSpecificRemoveInfoBar(
    infobars::InfoBar* infobar) {
  RemoveChildView(static_cast<InfoBarView*>(infobar));
}
