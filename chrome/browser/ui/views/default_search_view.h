// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEFAULT_SEARCH_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DEFAULT_SEARCH_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/tab_contents/constrained_window.h"
#include "views/window/dialog_delegate.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#endif

class PrefService;
class TabContents;
class TemplateURL;
class TemplateURLModel;

namespace gfx {
class Canvas;
}

namespace views {
class Button;
class ImageView;
class Label;
class View;
}

// Responsible for displaying the contents of the default search
// prompt for when InstallSearchProvider(url, true) is called.
class DefaultSearchView
    : public views::View,
      public views::ButtonListener,
      public ConstrainedDialogDelegate {
 public:
  // Takes ownership of |proposed_default_turl|.
  static void Show(TabContents* tab_contents,
                   TemplateURL* ,
                   TemplateURLModel* template_url_model);

  virtual ~DefaultSearchView();

 protected:
  // Overridden from views::View:
  // Draws the gray background at the top of the dialog.
  virtual void OnPaint(gfx::Canvas* canvas);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // ConstrainedDialogDelegate:
  virtual std::wstring GetWindowTitle() const;
  virtual views::View* GetInitiallyFocusedView();
  virtual views::View* GetContentsView();
  virtual int GetDialogButtons() const;
  virtual bool Accept();

 private:
  // Takes ownership of |proposed_default_turl|.
  DefaultSearchView(TabContents* tab_contents,
                    TemplateURL* proposed_default_turl,
                    TemplateURLModel* template_url_model);

  // Initializes the labels and controls in the view.
  void SetupControls(PrefService* prefs);

  // Image of browser search box with grey background and bubble arrow.
  views::ImageView* background_image_;

  // Button for the current default search engine.
  views::View* default_provider_button_;

  // Button for the newly proposed search engine.
  views::View* proposed_provider_button_;

  // The proposed new default search engine.
  scoped_ptr<TemplateURL> proposed_turl_;

  TemplateURLModel* template_url_model_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DEFAULT_SEARCH_VIEW_H_
