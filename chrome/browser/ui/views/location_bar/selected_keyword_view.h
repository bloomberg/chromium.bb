// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SELECTED_KEYWORD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SELECTED_KEYWORD_VIEW_H_

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "ui/views/controls/label.h"

class Profile;
namespace gfx {
class FontList;
class Size;
}

// SelectedKeywordView displays the tab-to-search UI in the location bar view.
class SelectedKeywordView : public IconLabelBubbleView {
 public:
  SelectedKeywordView(const gfx::FontList& font_list,
                      SkColor text_color,
                      SkColor parent_background_color,
                      Profile* profile);
  virtual ~SelectedKeywordView();

  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;

  // The current keyword, or an empty string if no keyword is displayed.
  void SetKeyword(const base::string16& keyword);
  const base::string16& keyword() const { return keyword_; }

 private:
  // The keyword we're showing. If empty, no keyword is selected.
  // NOTE: we don't cache the TemplateURL as it is possible for it to get
  // deleted out from under us.
  base::string16 keyword_;

  // These labels are never visible.  They are used to size the view.  One
  // label contains the complete description of the keyword, the second
  // contains a truncated version of the description, for if there is not
  // enough room to display the complete description.
  views::Label full_label_;
  views::Label partial_label_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SelectedKeywordView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SELECTED_KEYWORD_VIEW_H_
