// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container_view.h"

#include "base/message_loop.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "grit/generated_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"

// static
const char InfoBarContainerView::kViewClassName[] = "InfoBarContainerView";

InfoBarContainerView::InfoBarContainerView(Delegate* delegate,
                                           SearchModel* search_model)
    : InfoBarContainer(delegate, search_model) {
  set_id(VIEW_ID_INFO_BAR_CONTAINER);
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

std::string InfoBarContainerView::GetClassName() const {
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

void InfoBarContainerView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_INFOBAR_CONTAINER);
}

void InfoBarContainerView::PlatformSpecificAddInfoBar(InfoBar* infobar,
                                                      size_t position) {
  AddChildViewAt(static_cast<InfoBarView*>(infobar),
                 static_cast<int>(position));
}

void InfoBarContainerView::PlatformSpecificRemoveInfoBar(InfoBar* infobar) {
  RemoveChildView(static_cast<InfoBarView*>(infobar));
  MessageLoop::current()->DeleteSoon(FROM_HERE, infobar);
}
