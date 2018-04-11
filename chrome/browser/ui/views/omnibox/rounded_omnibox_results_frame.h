// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_ROUNDED_OMNIBOX_RESULTS_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_ROUNDED_OMNIBOX_RESULTS_FRAME_H_

#include <memory>

#include "ui/gfx/geometry/insets.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

class LocationBarView;

// A class that wraps a Widget's content view to provide a custom results frame.
class RoundedOmniboxResultsFrame : public views::View {
 public:
  // How the Widget is aligned relative to the location bar.
  static constexpr gfx::Insets kLocationBarAlignmentInsets =
      gfx::Insets(6, 6, 0, 6);

  RoundedOmniboxResultsFrame(views::View* contents,
                             LocationBarView* location_bar);
  ~RoundedOmniboxResultsFrame() override;

  // Hook to customize Widget initialization.
  static void OnBeforeWidgetInit(views::Widget::InitParams* params);

  // Insets applied to the browser location bar to determine where the location
  // and width of the top edge of the frame should appear.
  static gfx::Insets GetAlignmentInsets(views::View* location_bar);

  // views::View:
  const char* GetClassName() const override;
  void Layout() override;
  void AddedToWidget() override;

 private:
  // Fixed layout constants.
  const gfx::Insets content_insets_;
  const int location_bar_height_;
  const int separator_inset_;

  std::unique_ptr<ui::LayerOwner> contents_mask_;

  views::View* top_background_ = nullptr;
  views::View* separator_ = nullptr;
  views::View* contents_ = nullptr;
  views::View* contents_host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RoundedOmniboxResultsFrame);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_ROUNDED_OMNIBOX_RESULTS_FRAME_H_
