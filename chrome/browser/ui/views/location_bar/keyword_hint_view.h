// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_KEYWORD_HINT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_KEYWORD_HINT_VIEW_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "ui/gfx/size.h"
#include "views/view.h"

namespace gfx {
class Font;
}
class Profile;
namespace views {
class Label;
}

// KeywordHintView is used by the location bar view to display a hint to the
// user when the selected url has a corresponding keyword.
//
// Internally KeywordHintView uses two labels to render the text, and draws
// the tab image itself.
//
// NOTE: This should really be called LocationBarKeywordHintView, but I
// couldn't bring myself to use such a long name.
class KeywordHintView : public views::View {
 public:
  explicit KeywordHintView(Profile* profile);
  virtual ~KeywordHintView();

  void SetFont(const gfx::Font& font);

  void SetKeyword(const string16& keyword);
  string16 keyword() const { return keyword_; }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  // The minimum size is just big enough to show the tab.
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  views::Label* CreateLabel();

  views::Label* leading_label_;
  views::Label* trailing_label_;

  // The keyword.
  string16 keyword_;

  Profile* profile_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(KeywordHintView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_KEYWORD_HINT_VIEW_H_
