// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"

#include "ui/gfx/color_palette.h"

gfx::Insets GetToolbarInkDropInsets(const views::View* host_view,
                                    const gfx::Insets& margin_insets) {
  // TODO(pbos): Inkdrop masks and layers should be flipped with RTL. Fix this
  // and remove RTL handling from here.
  gfx::Insets inkdrop_insets =
      base::i18n::IsRTL()
          ? gfx::Insets(margin_insets.top(), margin_insets.right(),
                        margin_insets.bottom(), margin_insets.left())
          : margin_insets;

  // Inset the inkdrop insets so that the end result matches the target inkdrop
  // dimensions.
  const gfx::Size host_size = host_view->size();
  const int inkdrop_dimensions = GetLayoutConstant(LOCATION_BAR_HEIGHT);
  inkdrop_insets += gfx::Insets((host_size.height() - inkdrop_dimensions) / 2);

  return inkdrop_insets;
}

std::unique_ptr<gfx::Path> CreateToolbarHighlightPath(
    const views::View* host_view,
    const gfx::Insets& margin_insets) {
  gfx::Rect rect(host_view->size());
  rect.Inset(GetToolbarInkDropInsets(host_view, margin_insets));

  const int radii = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
      views::EMPHASIS_MAXIMUM, rect.size());

  auto path = std::make_unique<gfx::Path>();
  path->addRoundRect(gfx::RectToSkRect(rect), radii, radii);
  return path;
}

SkColor GetToolbarInkDropBaseColor(const views::View* host_view) {
  const auto* theme_provider = host_view->GetThemeProvider();
  // There may be no theme provider in unit tests.
  if (theme_provider) {
    return color_utils::BlendTowardOppositeLuma(
        theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR),
        SK_AlphaOPAQUE);
  }

  return gfx::kPlaceholderColor;
}
