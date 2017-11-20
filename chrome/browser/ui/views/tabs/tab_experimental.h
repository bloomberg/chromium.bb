// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_H_

#include "base/macros.h"
#include "chrome/browser/ui/tabs/tab_data_experimental.h"
#include "chrome/browser/ui/views/tabs/tab_experimental_paint.h"
#include "ui/views/controls/glow_hover_controller.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/view.h"

class TabDataExperimental;
class TabStripModelExperimental;

namespace views {
class Label;
}

class TabExperimental : public views::MaskedTargeterDelegate,
                        public views::View {
 public:
  explicit TabExperimental(TabStripModelExperimental* model,
                           const TabDataExperimental* data);
  ~TabExperimental() override;

  // Will be null when closing.
  const TabDataExperimental* data() const { return data_; }

  // Cached stuff from data_ that can be used regardless of whether the data
  // pointer is null or not.
  TabDataExperimental::Type type() const { return type_; }

  // The view order is stored on a tab for the use of TabStrip layout and
  // computatations. It is not used directly by this class. It represents this
  // tab's position in the current layout.
  size_t view_order() const { return view_order_; }
  void set_view_order(size_t vo) { view_order_ = vo; }

  // The ideal bounds is stored on a tab for the use of the TabStrip layout.
  // It is not used directly by this class.
  const gfx::Rect& ideal_bounds() const { return ideal_bounds_; }
  void set_ideal_bounds(const gfx::Rect& r) { ideal_bounds_ = r; }

  // Used to set/check whether this Tab is being animated closed. Marking a
  // tab as closing will also clear the data_ pointer.
  void SetClosing();
  bool closing() const { return closing_; }

  bool active() const { return active_; }
  bool selected() const { return selected_; }

  // FIXME(brettw) do we want an equivalent to TabRendererData to set
  // everything at once?
  void SetActive(bool active);
  void SetSelected(bool selected);

  // TODO(brettw) need a way to specify what changed so the tab doesn't need
  // to redraw everything.
  void DataUpdated();

  // Called for group types when layout is done to set the bounds of the
  // first tab. This is used to determine some painting parameters.
  void SetGroupLayoutParams(int first_child_begin_x);

  // Returns the overlap between adjacent tabs.
  static int GetOverlap();

 private:
  // views::MaskedTargeterDelegate:
  bool GetHitTestMask(gfx::Path* mask) const override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;

  TabStripModelExperimental* model_;

  // Will be null when closing.
  const TabDataExperimental* data_;

  TabDataExperimental::Type type_;

  size_t view_order_ = static_cast<size_t>(-1);
  gfx::Rect ideal_bounds_;

  bool active_ = false;
  bool selected_ = false;
  bool closing_ = false;

  views::Label* title_;  // Non-owning (owned by View hierarchy).

  // Location of the first child tab. Negative indicates unused.
  int first_child_begin_x_ = -1;

  views::GlowHoverController hover_controller_;

  TabExperimentalPaint paint_;

  DISALLOW_COPY_AND_ASSIGN(TabExperimental);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_H_
