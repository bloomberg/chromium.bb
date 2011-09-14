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
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/gfx/native_widget_types.h"

class CustomDrawButton;
class GURL;
class TabContents;
class WebIntentController;
class WebIntentPickerDelegate;

// Gtk implementation of WebIntentPicker.
class WebIntentPickerGtk : public WebIntentPicker,
                           public BubbleDelegateGtk {
 public:
  WebIntentPickerGtk(gfx::NativeWindow parent,
                     TabContentsWrapper* tab_contents,
                     WebIntentPickerDelegate* delegate);
  virtual ~WebIntentPickerGtk();

  // WebIntentPicker implementation.
  virtual void SetServiceURLs(const std::vector<GURL>& urls) OVERRIDE;
  virtual void SetServiceIcon(size_t index, const SkBitmap& icon) OVERRIDE;
  virtual void SetDefaultServiceIcon(size_t index) OVERRIDE;
  virtual void Close() OVERRIDE;

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

 private:
  // Callback when a service button is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnServiceButtonClick);
  // Callback when close button is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnCloseButtonClick);

  // Initialize the contents of the bubble. After this call, contents_ will be
  // non-NULL.
  void InitContents();

  // A weak pointer to the tab contents on which to display the picker UI.
  TabContentsWrapper* wrapper_;

  // A weak pointer to the WebIntentPickerDelegate to notify when the user
  // chooses a service or cancels.
  WebIntentPickerDelegate* delegate_;

  // A weak pointer to the widget that contains all other widgets in
  // the bubble.
  GtkWidget* contents_;

  // A weak pointer to the hbox that contains the buttons used to choose the
  // service.
  GtkWidget* button_hbox_;

  // A button to close the bubble.
  scoped_ptr<CustomDrawButton> close_button_;

  // A weak pointer to the bubble widget.
  BubbleGtk* bubble_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_WEB_INTENT_PICKER_GTK_H_
