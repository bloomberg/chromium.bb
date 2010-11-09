// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/gtk/constrained_window_gtk.h"
#endif

class MessageBoxView;
class TabContents;
class TemplateURL;
class TemplateURLModel;

namespace views {
class View;
}

// This class is responsible for displaying the contents of the default search
// prompt for when InstallSearchProvider(url, true) is called.
class DefaultSearchView : public ConstrainedDialogDelegate {
 public:
  static void Show(TabContents* tab_contents,
                   TemplateURL* default_url,
                   TemplateURLModel* template_url_model);

 protected:
  // ConstrainedDialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual std::wstring GetWindowTitle() const;
  virtual void DeleteDelegate();
  virtual views::View* GetContentsView();
  virtual int GetDefaultDialogButton() const;
  virtual bool Accept();

 private:
  DefaultSearchView(TabContents* tab_contents,
                    TemplateURL* default_url,
                    TemplateURLModel* template_url_model);
  ~DefaultSearchView();

  // The host name for the possible default search provider.
  string16 DefaultHostName() const;

  // The possible new default url.
  scoped_ptr<TemplateURL> default_url_;
  TemplateURLModel* template_url_model_;

  // The message box view whose commands we handle.
  MessageBoxView* message_box_view_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DEFAULT_SEARCH_VIEW_H_
