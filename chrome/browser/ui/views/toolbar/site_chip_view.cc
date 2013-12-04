// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/site_chip_view.h"

#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"

const int kEdgeThickness = 4;
const int kIconTextSpacing = 4;
const int kTrailingLabelMargin = 2;

SiteChipView::SiteChipView(ToolbarView* toolbar_view)
    : ToolbarButton(this, NULL),
      toolbar_view_(toolbar_view) {
}

SiteChipView::~SiteChipView() {
}

void SiteChipView::Init() {
  ToolbarButton::Init();

  // TODO(gbillock): Would be nice to just use stock LabelButton stuff here.
  location_icon_view_ = new LocationIconView(toolbar_view_->location_bar());
  host_label_ = new views::Label();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  host_label_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));

  if (!ShouldShow())
    return;

  AddChildView(location_icon_view_);
  AddChildView(host_label_);

  // temporary background painter
  const int kBackgroundImages[] = IMAGE_GRID(IDR_SITE_CHIP_EV);
  background_painter_.reset(
      views::Painter::CreateImageGridPainter(kBackgroundImages));

  // temporary icon filler
  location_icon_view_->SetImage(GetThemeProvider()->GetImageSkiaNamed(
      IDR_OMNIBOX_HTTPS_VALID));
  location_icon_view_->ShowTooltip(true);

  // temporary filler text.
  host_label_->SetText(ASCIIToUTF16("site.chip"));
}

bool SiteChipView::ShouldShow() {
  return chrome::ShouldDisplayOriginChip();
}

void SiteChipView::Update(content::WebContents* tab) {
  Layout();
  SchedulePaint();
}

gfx::Size SiteChipView::GetPreferredSize() {
  if (!ShouldShow())
    return gfx::Size();

  gfx::Size label_size = host_label_->GetPreferredSize();
  gfx::Size icon_size = location_icon_view_->GetPreferredSize();
  return gfx::Size(icon_size.width() + label_size.width() +
                   kIconTextSpacing + kTrailingLabelMargin +
                   2 * kEdgeThickness,
                   icon_size.height());
}

void SiteChipView::Layout() {
  location_icon_view_->SetBounds(
      kEdgeThickness,
      LocationBarView::kNormalEdgeThickness,
      location_icon_view_->GetPreferredSize().width(),
      height() - 2 * LocationBarView::kNormalEdgeThickness);

  int host_label_x = location_icon_view_->x() + location_icon_view_->width() +
      kIconTextSpacing;
  int host_label_width =
      width() - host_label_x - kEdgeThickness - kTrailingLabelMargin;
  host_label_->SetBounds(host_label_x,
                         LocationBarView::kNormalEdgeThickness,
                         host_label_width,
                         height() - 2 * LocationBarView::kNormalEdgeThickness);
}

void SiteChipView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetLocalBounds());
  rect.Inset(LocationBarView::kNormalEdgeThickness,
             LocationBarView::kNormalEdgeThickness);
  if (background_painter_.get())
    views::Painter::PaintPainterAt(canvas, background_painter_.get(), rect);

  ToolbarButton::OnPaint(canvas);
}

// TODO(gbillock): Make the LocationBarView or OmniboxView the listener for
// this button.
void SiteChipView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  toolbar_view_->location_bar()->GetOmniboxView()->SetFocus();
  toolbar_view_->location_bar()->GetOmniboxView()->SelectAll(true);
  toolbar_view_->location_bar()->GetOmniboxView()->model()->
      SetCaretVisibility(true);
}
