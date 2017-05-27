// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_KEYWORD_HINT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_KEYWORD_HINT_VIEW_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/custom_button.h"

class Profile;

namespace gfx {
class FontList;
}

namespace views {
class Label;
}

// KeywordHintView is used by the location bar view to display a hint to the
// user that pressing Tab will enter tab to search mode. The view is also
// clickable, which has the same effect as pressing tab.
//
// Internally KeywordHintView uses two labels to render the text, and draws
// the tab image itself.
//
// NOTE: This should really be called LocationBarKeywordHintView, but I
// couldn't bring myself to use such a long name.
class KeywordHintView : public views::CustomButton {
 public:
  KeywordHintView(views::ButtonListener* listener,
                  Profile* profile,
                  const gfx::FontList& font_list,
                  const gfx::FontList& chip_font_list,
                  SkColor text_color,
                  SkColor background_color);
  ~KeywordHintView() override;

  void SetKeyword(const base::string16& keyword);
  base::string16 keyword() const { return keyword_; }

 private:
  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  // The minimum size is just big enough to show the tab.
  gfx::Size GetMinimumSize() const override;
  void Layout() override;
  const char* GetClassName() const override;

  views::Label* CreateLabel(const gfx::FontList& font_list,
                            SkColor text_color,
                            SkColor background_color);

  Profile* profile_;

  views::Label* leading_label_;
  views::View* chip_container_;
  views::Label* chip_label_;
  views::Label* trailing_label_;

  base::string16 keyword_;

  DISALLOW_COPY_AND_ASSIGN(KeywordHintView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_KEYWORD_HINT_VIEW_H_
