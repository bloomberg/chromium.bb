// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_WEB_INTENT_PICKER_GTK_H_
#define CHROME_BROWSER_UI_GTK_WEB_INTENT_PICKER_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class Browser;
class CustomDrawButton;
class GURL;
class TabContents;
class TabContentsContainerGtk;
class TabContentsWrapper;
class WebIntentPickerDelegate;

// Gtk implementation of WebIntentPicker.
class WebIntentPickerGtk : public WebIntentPicker,
                           public WebIntentPickerModelObserver,
                           public BubbleDelegateGtk {
 public:
  WebIntentPickerGtk(Browser* browser,
                     TabContentsWrapper* tab_contents,
                     WebIntentPickerDelegate* delegate,
                     WebIntentPickerModel* model);
  virtual ~WebIntentPickerGtk();

  // WebIntentPicker implementation.
  virtual void Close() OVERRIDE;

  // WebIntentPickerModelObserver implementation.
  virtual void OnModelChanged(WebIntentPickerModel* model) OVERRIDE;
  virtual void OnFaviconChanged(WebIntentPickerModel* model,
                                size_t index) OVERRIDE;
  virtual void OnInlineDisposition(WebIntentPickerModel* model) OVERRIDE;

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

 private:
  // Callback when picker is destroyed.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnDestroy);
  // Callback when a service button is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnServiceButtonClick);
  // Callback when close button is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnCloseButtonClick);

  // This class is the policy delegate for the rendered page in the intents
  // inline disposition bubble.
  // TODO(gbillock): Move up to WebIntentPicker?
  class InlineDispositionDelegate : public content::WebContentsDelegate {
   public:
    InlineDispositionDelegate();
    virtual ~InlineDispositionDelegate();
    virtual bool IsPopupOrPanel(
        const content::WebContents* source) const OVERRIDE;
    virtual bool ShouldAddNavigationToHistory(
      const history::HistoryAddPageArgs& add_page_args,
      content::NavigationType navigation_type) OVERRIDE;
  };

  // Initialize the contents of the bubble. After this call, contents_ will be
  // non-NULL.
  void InitContents();

  // Get the button widget at |index|.
  GtkWidget* GetServiceButton(size_t index);

  // A weak pointer to the tab contents on which to display the picker UI.
  TabContentsWrapper* wrapper_;

  // A weak pointer to the WebIntentPickerDelegate to notify when the user
  // chooses a service or cancels.
  WebIntentPickerDelegate* delegate_;

  // A weak pointer to the picker model.
  WebIntentPickerModel* model_;

  // A weak pointer to the widget that contains all other widgets in
  // the bubble.
  GtkWidget* contents_;

  // A weak pointer to the vbox that contains the buttons used to choose the
  // service.
  GtkWidget* button_vbox_;

  // A button to close the bubble.
  scoped_ptr<CustomDrawButton> close_button_;

  // A weak pointer to the bubble widget.
  BubbleGtk* bubble_;

  // The browser we're in.
  Browser* browser_;

  // Container for the HTML in the inline disposition case.
  scoped_ptr<TabContentsWrapper> inline_disposition_tab_contents_;

  // Widget for displaying the HTML in the inline disposition case.
  scoped_ptr<TabContentsContainerGtk> tab_contents_container_;

  // content::WebContentsDelegate for the inline disposition dialog.
  scoped_ptr<InlineDispositionDelegate> inline_disposition_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_WEB_INTENT_PICKER_GTK_H_
