// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_HTML_DIALOG_GTK_H_
#define CHROME_BROWSER_GTK_HTML_DIALOG_GTK_H_

#include <string>
#include <vector>

#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"

typedef struct _GtkWidget GtkWidget;

class TabContents;
class TabContentsContainerGtk;

class HtmlDialogGtk : public TabContentsDelegate,
                      public HtmlDialogUIDelegate {
 public:
  HtmlDialogGtk(Browser* parent_browser, HtmlDialogUIDelegate* delegate);
  virtual ~HtmlDialogGtk();

  static void ShowHtmlDialogGtk(Browser* browser,
                                HtmlDialogUIDelegate* delegate);
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
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void ReplaceContents(TabContents* source,
                               TabContents* new_contents);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void URLStarredChanged(TabContents* source, bool starred);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);

 private:
  static void OnResponse(GtkWidget* widget, int response,
                         HtmlDialogGtk* dialog);

  // The Browser object which created this html dialog; we send all
  // window opening/navigations to this object.
  Browser* parent_browser_;

  // This view is a delegate to the HTML content since it needs to get notified
  // about when the dialog is closing. For all other actions (besides dialog
  // closing) we delegate to the creator of this view, which we keep track of
  // using this variable.
  HtmlDialogUIDelegate* delegate_;

  GtkWidget* dialog_;

  scoped_ptr<TabContents> tab_contents_;
  scoped_ptr<TabContentsContainerGtk> tab_contents_container_;

  DISALLOW_COPY_AND_ASSIGN(HtmlDialogGtk);
};

#endif  // CHROME_BROWSER_GTK_HTML_DIALOG_GTK_H_
