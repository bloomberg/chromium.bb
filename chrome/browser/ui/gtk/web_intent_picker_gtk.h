// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_WEB_INTENT_PICKER_GTK_H_
#define CHROME_BROWSER_UI_GTK_WEB_INTENT_PICKER_GTK_H_
#pragma once

#include <vector>

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "ui/base/gtk/gtk_signal.h"

class CustomDrawButton;
class GURL;
class TabContents;
class WebIntentController;
class WebIntentPickerDelegate;

// Gtk implementation of WebIntentPicker.
class WebIntentPickerGtk : public WebIntentPicker,
                           public ConstrainedDialogDelegate {
 public:
  WebIntentPickerGtk(TabContents* tab_contents,
                     WebIntentPickerDelegate* delegate);
  virtual ~WebIntentPickerGtk();

  // WebIntentPicker implementation.
  virtual void SetServiceURLs(const std::vector<GURL>& urls) OVERRIDE;
  virtual void SetServiceIcon(size_t index, const SkBitmap& icon) OVERRIDE;
  virtual void SetDefaultServiceIcon(size_t index) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Close() OVERRIDE;

  // ConstrainedDialogDelegate implementation.
  virtual GtkWidget* GetWidgetRoot() OVERRIDE;
  virtual GtkWidget* GetFocusWidget() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

 private:
  // Callback when a service button is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnServiceButtonClick);
  // Callback when close button is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnCloseButtonClick);

  // A weak pointer to the tab contents on which to display the picker UI.
  TabContents* tab_contents_;

  // A weak pointer to the WebIntentPickerDelegate to notify when the user
  // chooses a service or cancels.
  WebIntentPickerDelegate* delegate_;

  // The root GtkWidget of the dialog.
  ui::OwnedWidgetGtk root_;

  // A weak pointer to the hbox that contains the buttons used to choose the
  // service.
  GtkWidget* button_hbox_;

  // A button to close the dialog delegate.
  scoped_ptr<CustomDrawButton> close_button_;

  // The displayed constrained window. Technically owned by the TabContents
  // page, but this object tells it when to destroy.
  ConstrainedWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_WEB_INTENT_PICKER_GTK_H_
