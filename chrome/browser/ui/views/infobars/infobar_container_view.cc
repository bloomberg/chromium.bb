// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container_view.h"

#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/ui/infobar_container_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view_targeter.h"

namespace {

class ContentShadow : public views::View {
 public:
  ContentShadow();

 protected:
  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
};

ContentShadow::ContentShadow() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

gfx::Size ContentShadow::CalculatePreferredSize() const {
  return gfx::Size(0, views::BubbleBorder::GetBorderAndShadowInsets().height());
}

void ContentShadow::OnPaint(gfx::Canvas* canvas) {
  // Outdent the sides to make the shadow appear uniform in the corners.
  gfx::RectF container_bounds(parent()->GetLocalBounds());
  View::ConvertRectToTarget(parent(), this, &container_bounds);
  container_bounds.Inset(-views::BubbleBorder::kShadowBlur, 0);

  views::BubbleBorder::DrawBorderAndShadow(gfx::RectFToSkRect(container_bounds),
                                           &cc::PaintCanvas::drawRect, canvas);
}

}  // namespace

// static
const char InfoBarContainerView::kViewClassName[] = "InfoBarContainerView";

InfoBarContainerView::InfoBarContainerView(Delegate* delegate)
    : infobars::InfoBarContainer(delegate),
      content_shadow_(new ContentShadow()) {
  set_id(VIEW_ID_INFO_BAR_CONTAINER);
  AddChildView(content_shadow_);
}

InfoBarContainerView::~InfoBarContainerView() {
  RemoveAllInfoBarsForDestruction();
}

gfx::Size InfoBarContainerView::CalculatePreferredSize() const {
  int total_height;
  int overlap = GetVerticalOverlap(&total_height);
  total_height -= overlap;

  // No need to reserve space for the bottom bar's separator; the shadow is good
  // enough.
  total_height -= InfoBarContainerDelegate::kSeparatorLineHeight;

  gfx::Size size(0, total_height);
  for (int i = 0; i < child_count(); ++i)
    size.SetToMax(gfx::Size(child_at(i)->GetPreferredSize().width(), 0));

  // Don't reserve space for the bottom shadow here.  Because the shadow paints
  // to its own layer and this class doesn't, it can paint outside the size
  // computed here.  Not including the shadow bounds means the browser will
  // automatically lay out web content beginning below the bottom infobar
  // (instead of below the shadow), and clicks in the shadow region will go to
  // the web content instead of the infobars; both of these effects are
  // desirable.  On the other hand, it also means the browser doesn't know the
  // shadow is there and could lay out something atop it or size the window too
  // small for it; but these are unlikely.
  return size;
}

const char* InfoBarContainerView::GetClassName() const {
  return kViewClassName;
}

void InfoBarContainerView::Layout() {
  int top = 0;

  for (int i = 0; i < child_count() - 1; ++i) {
    InfoBarView* child = static_cast<InfoBarView*>(child_at(i));
    top -= child->arrow_height();
    int child_height = child->total_height();

    // Trim off the bottom bar's separator; the shadow is good enough.
    // The last infobar is the second to last child overall (followed by
    // |content_shadow_|).
    if (i == child_count() - 2)
      child_height -= InfoBarContainerDelegate::kSeparatorLineHeight;
    child->SetBounds(0, top, width(), child_height);
    top += child_height;
  }

  // The shadow is positioned flush with the bottom infobar, with the separator
  // there drawn by the shadow code (so we don't have to extend our bounds out
  // to be able to draw it; see comments in CalculatePreferredSize() on why the
  // shadow is drawn outside the container bounds).
  content_shadow_->SetBounds(0, top, width(),
                             content_shadow_->GetPreferredSize().height());
}

void InfoBarContainerView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGroup;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_INFOBAR_CONTAINER));
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
