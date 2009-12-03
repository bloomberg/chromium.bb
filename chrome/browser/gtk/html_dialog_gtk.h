// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_HTML_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_HTML_DIALOG_GTK_H_

#include <string>
#include <vector>

#include "app/gfx/native_widget_types.h"
#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/dom_ui/html_dialog_tab_contents_delegate.h"

typedef struct _GtkWidget GtkWidget;

class Browser;
class Profile;
class TabContents;
class TabContentsContainerGtk;

class HtmlDialogGtk : public HtmlDialogTabContentsDelegate,
                      public HtmlDialogUIDelegate {
 public:
  HtmlDialogGtk(Profile* profile, HtmlDialogUIDelegate* delegate,
                gfx::NativeWindow parent_window);
  virtual ~HtmlDialogGtk();

  static void ShowHtmlDialogGtk(Browser* browser,
                                HtmlDialogUIDelegate* delegate,
                                gfx::NativeWindow parent_window);
  // Initializes the contents of the dialog (the DOMView and the callbacks).
  void InitDialog();

  // Overridden from HtmlDialogUI::Delegate:
  virtual bool IsDialogModal() const;
  virtual std::wstring GetDialogTitle() const;
  virtual GURL GetDialogContentURL() const;
  virtual void GetDOMMessageHandlers(
      std::vector<DOMMessageHandler*>* handlers) const;
  virtual void GetDialogSize(gfx::Size* size) const;
  virtual std::string GetDialogArgs() const;
  virtual void OnDialogClosed(const std::string& json_retval);

  // Overridden from TabContentsDelegate:
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);

 private:
  static void OnResponse(GtkWidget* widget, int response,
                         HtmlDialogGtk* dialog);

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  HtmlDialogUIDelegate* delegate_;

  gfx::NativeWindow parent_window_;

  GtkWidget* dialog_;

  scoped_ptr<TabContents> tab_contents_;
  scoped_ptr<TabContentsContainerGtk> tab_contents_container_;

  DISALLOW_COPY_AND_ASSIGN(HtmlDialogGtk);
};

#endif  // CHROME_BROWSER_GTK_HTML_DIALOG_GTK_H_
