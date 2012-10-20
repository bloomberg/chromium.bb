// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SEARCH_SEARCH_NTP_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SEARCH_SEARCH_NTP_CONTAINER_VIEW_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/search/search_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"

class Profile;

namespace views {
class WebView;
}

namespace internal {

// This class implements one of the child views of |SearchViewController| that
// displays the search provider logo and content view.  It also fires
// notification of y-pos changes of theme background image in content view.
class SearchNTPContainerView : public views::View {
 public:
  SearchNTPContainerView(Profile* profile, views::View* browser_view);
  virtual ~SearchNTPContainerView();

  // Removes previous logo view if available, adds the new logo view as child
  // view.
  void SetLogoView(views::View* logo_view);

  // Removes previous content view if available, adds the new content view as
  // child view.
  void SetContentView(views::WebView* content_view);

  // Sends notification for CSS NTP background y-pos if it has changed from what
  // was last notified.
  void NotifyNTPBackgroundYPosIfChanged(views::WebView* content_view,
                                        const chrome::search::Mode& mode);

  // views::View overrides:
  virtual void Layout() OVERRIDE;

  void set_to_notify_css_background_y_pos() {
    to_notify_css_background_y_pos_ = true;
  }

 protected:
  // views::View overrides:
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

 private:
  Profile* profile_;  // Weak.

  views::View* browser_view_;  // Weak.

  views::View* logo_view_;

  views::WebView* content_view_;

  // The CSS background y pos that was last notified via
  // chrome::NOTIFICATION_NTP_BACKGROUND_THEME_Y_POS_CHANGED.
  int notified_css_background_y_pos_;

  // Set to true to notify of CSS background y pos regardless if it has changed;
  // otherwise, it's only notified if it has changed.
  bool to_notify_css_background_y_pos_;

  DISALLOW_COPY_AND_ASSIGN(SearchNTPContainerView);
};

}  // namespace internal

#endif  // CHROME_BROWSER_UI_VIEWS_SEARCH_SEARCH_NTP_CONTAINER_VIEW_H_
