// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEFAULT_SEARCH_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DEFAULT_SEARCH_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "content/browser/tab_contents/constrained_window.h"
#include "views/window/dialog_delegate.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#endif

class PrefService;
class TabContents;
class TemplateURL;
class TemplateURLService;

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
      public views::DialogDelegate {
 public:
  // Takes ownership of |proposed_default_turl|.
  static void Show(TabContents* tab_contents,
                   TemplateURL* ,
                   TemplateURLService* template_url_service);

  virtual ~DefaultSearchView();

 protected:
  // Overridden from views::View:
  // Draws the gray background at the top of the dialog.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::DialogDelegate:
  // TODO(beng): Figure out why adding OVERRIDE to these methods annoys Clang.
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

 private:
  // Takes ownership of |proposed_default_turl|.
  DefaultSearchView(TabContents* tab_contents,
                    TemplateURL* proposed_default_turl,
                    TemplateURLService* template_url_service);

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

  TemplateURLService* template_url_service_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DEFAULT_SEARCH_VIEW_H_
