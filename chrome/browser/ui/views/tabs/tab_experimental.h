// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/tabs/tab_experimental_paint.h"
#include "ui/views/controls/glow_hover_controller.h"
#include "ui/views/view.h"

namespace views {
class Label;
}

class TabExperimental : public views::View {
 public:
  TabExperimental();
  ~TabExperimental() override;

  // Used to set/check whether this Tab is being animated closed.
  void set_closing(bool closing) { closing_ = closing; }
  bool closing() const { return closing_; }

  bool active() const { return active_; }
  bool selected() const { return selected_; }

  // FIXME(brettw) do we want an equivalent to TabRendererData to set
  // everything at once?
  void SetActive(bool active);
  void SetSelected(bool selected);
  void SetTitle(const base::string16& title);

  // Returns the overlap between adjacent tabs.
  static int GetOverlap();

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;

  bool active_ = false;
  bool selected_ = false;
  bool closing_ = false;

  views::Label* title_;  // Non-owning (owned by View hierarchy).

  views::GlowHoverController hover_controller_;

  TabExperimentalPaint paint_;

  DISALLOW_COPY_AND_ASSIGN(TabExperimental);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_EXPERIMENTAL_H_
