// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/managed_prefs_banner_view.h"

#include "gfx/color_utils.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/layout/box_layout.h"
#include "views/standard_layout.h"

// Spacing between the banner frame and its contents.
static const int kPrefsBannerPadding = 3;
// Width of the banner frame.
static const int kPrefsBannerBorderSize = 1;

ManagedPrefsBannerView::ManagedPrefsBannerView(PrefService* prefs,
                                               OptionsPage page)
    : policy::ManagedPrefsBannerBase(prefs, page) {
  content_ = new views::View;
  SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
  views::Border* border = views::Border::CreateSolidBorder(
      kPrefsBannerBorderSize, border_color);
  content_->set_border(border);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  warning_image_ = new views::ImageView();
  warning_image_->SetImage(rb.GetBitmapNamed(IDR_WARNING));
  label_ = new views::Label(rb.GetLocalizedString(IDS_OPTIONS_MANAGED_PREFS));
}

void ManagedPrefsBannerView::Init() {
  AddChildView(content_);
  content_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           kPrefsBannerPadding,
                           kPrefsBannerPadding,
                           kRelatedControlSmallHorizontalSpacing));
  content_->AddChildView(warning_image_);
  content_->AddChildView(label_);
  OnUpdateVisibility();
}

gfx::Size ManagedPrefsBannerView::GetPreferredSize() {
  if (!IsVisible())
    return gfx::Size();

  // Add space below the banner.
  gfx::Size size(content_->GetPreferredSize());
  size.Enlarge(0, kRelatedControlVerticalSpacing);
  return size;
}

void ManagedPrefsBannerView::Layout() {
  content_->SetBounds(0, 0, width(), height() - kRelatedControlVerticalSpacing);
}

void ManagedPrefsBannerView::ViewHierarchyChanged(bool is_add,
                                                  views::View* parent,
                                                  views::View* child) {
  if (is_add && child == this)
    Init();
}

void ManagedPrefsBannerView::OnUpdateVisibility() {
  SetVisible(DetermineVisibility());
}
