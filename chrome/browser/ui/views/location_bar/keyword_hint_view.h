// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_KEYWORD_HINT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_KEYWORD_HINT_VIEW_H_
#pragma once

#include <string>

#include "gfx/size.h"
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

  void SetColor(const SkColor& color);

  void SetKeyword(const std::wstring& keyword);
  std::wstring keyword() const { return keyword_; }

  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  // The minimum size is just big enough to show the tab.
  virtual gfx::Size GetMinimumSize();
  virtual void Layout();

  void set_profile(Profile* profile) { profile_ = profile; }

 private:
  views::Label* leading_label_;
  views::Label* trailing_label_;

  // The keyword.
  std::wstring keyword_;

  Profile* profile_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(KeywordHintView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_KEYWORD_HINT_VIEW_H_
