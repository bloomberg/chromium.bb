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
#include "ui/views/view_targeter.h"

namespace {

// The content shadow is drawn in two stages. A darker, shorter shadow is
// blended with a taller, lighter shadow. The heights are in dp.
const int kSmallShadowHeight = 1;
const int kLargeShadowHeight = 3;
const SkAlpha kSmallShadowAlpha = 0x33;
const SkAlpha kLargeShadowAlpha = 0x1A;

class ContentShadow : public views::View {
 public:
  ContentShadow() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }
  ~ContentShadow() override {}

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    // The first shader (small shadow) blurs from 0 to kSmallShadowHeight.
    cc::PaintFlags flags;
    flags.setShader(gfx::CreateGradientShader(
        0, kSmallShadowHeight, SkColorSetA(SK_ColorBLACK, kSmallShadowAlpha),
        SkColorSetA(SK_ColorBLACK, SK_AlphaTRANSPARENT)));
    gfx::Rect small_shadow_bounds = GetLocalBounds();
    small_shadow_bounds.set_height(kSmallShadowHeight);
    canvas->DrawRect(small_shadow_bounds, flags);

    // The second shader (large shadow) is solid from 0 to kSmallShadowHeight
    // (blending with the first shader) and then blurs from kSmallShadowHeight
    // to kLargeShadowHeight.
    flags.setShader(gfx::CreateGradientShader(
        kSmallShadowHeight, height(),
        SkColorSetA(SK_ColorBLACK, kLargeShadowAlpha),
        SkColorSetA(SK_ColorBLACK, SK_AlphaTRANSPARENT)));
    canvas->DrawRect(GetLocalBounds(), flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentShadow);
};

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

gfx::Size InfoBarContainerView::GetPreferredSize() const {
  int total_height;
  int overlap = GetVerticalOverlap(&total_height);
  total_height -= overlap;

  // No need to reserve space for the bottom bar's separator; the shadow is good
  // enough.
  total_height -= InfoBarContainerDelegate::kSeparatorLineHeight;

  gfx::Size size(0, total_height);
  for (int i = 0; i < child_count(); ++i)
    size.SetToMax(gfx::Size(child_at(i)->GetPreferredSize().width(), 0));
  return size;
}

const char* InfoBarContainerView::GetClassName() const {
  return kViewClassName;
}

void InfoBarContainerView::Layout() {
  int top = 0;

  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i) == content_shadow_)
      continue;

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

  content_shadow_->SetBounds(0, top, width(), kLargeShadowHeight);
}

void InfoBarContainerView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_GROUP;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_INFOBAR_CONTAINER));
}

void InfoBarContainerView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  // Infobars must be redrawn when the NativeTheme changes because
  // they have a border with color COLOR_TOOLBAR_BOTTOM_SEPARATOR,
  // which might have changed.
  for (int i = 0; i < child_count(); ++i)
    child_at(i)->SchedulePaint();
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
