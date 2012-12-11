// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_WEB_INTENT_PICKER_GTK_H_
#define CHROME_BROWSER_UI_GTK_WEB_INTENT_PICKER_GTK_H_

#include <gtk/gtk.h>

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "chrome/browser/ui/intents/web_intent_picker_model_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

namespace ui {
class GtkSignalRegistrar;
}

class CustomDrawButton;
class GURL;
class ThrobberGtk;
class WebIntentPickerDelegate;
class WaitingDialog;

// GTK implementation of WebIntentPicker.
class WebIntentPickerGtk : public WebIntentPicker,
                           public WebIntentPickerModelObserver,
                           public ConstrainedWindowGtkDelegate,
                           public content::NotificationObserver {
 public:
  WebIntentPickerGtk(content::WebContents* web_contents,
                     WebIntentPickerDelegate* delegate,
                     WebIntentPickerModel* model);
  virtual ~WebIntentPickerGtk();

  // WebIntentPicker implementation.
  virtual void Close() OVERRIDE;
  virtual void SetActionString(const string16& action) OVERRIDE;
  virtual void OnExtensionInstallSuccess(const std::string& id) OVERRIDE;
  virtual void OnExtensionInstallFailure(const std::string& id) OVERRIDE;
  virtual void OnInlineDispositionAutoResize(const gfx::Size& size) OVERRIDE;
  virtual gfx::Size GetMaxInlineDispositionSize() OVERRIDE;

  // WebIntentPickerModelObserver implementation.
  virtual void OnModelChanged(WebIntentPickerModel* model) OVERRIDE;
  virtual void OnFaviconChanged(WebIntentPickerModel* model,
                                size_t index) OVERRIDE;
  virtual void OnExtensionIconChanged(WebIntentPickerModel* model,
                                      const std::string& extension_id) OVERRIDE;
  virtual void OnInlineDisposition(const string16& title,
                                   const GURL& url) OVERRIDE;

  virtual void OnPendingAsyncCompleted() OVERRIDE;
  virtual void InvalidateDelegate() OVERRIDE;

  // ConstrainedWindowGtkDelegate implementation.
  virtual GtkWidget* GetWidgetRoot() OVERRIDE;
  virtual GtkWidget* GetFocusWidget() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool GetBackgroundColor(GdkColor* color) OVERRIDE;
  virtual bool ShouldHaveBorderPadding() const OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Callback when picker is destroyed.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnDestroy);
  // Callback when a service button is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnServiceButtonClick);
  // Callback when close button is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnCloseButtonClick);
  // Callback when suggested extension title link is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnExtensionLinkClick);
  // Callback when suggested extension install button is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnExtensionInstallButtonClick);
  // Callback when "more suggestions" link is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnMoreSuggestionsLinkClick);
  // Callback when "or choose another service" link is clicked.
  CHROMEGTK_CALLBACK_0(WebIntentPickerGtk, void, OnChooseAnotherServiceClick);
  // Callback when the host tab contents size changes.
  CHROMEGTK_CALLBACK_1(WebIntentPickerGtk, void, OnHostContentsSizeAllocate,
                       GdkRectangle*);

  // Initialize the contents of the picker. After this call, contents_ will be
  // non-NULL.
  void InitContents();

  // Initialize the main picker dialog.
  void InitMainContents();

  // Clear all contents and reset related variables.
  void ClearContents();

  // Reset contents to the initial picker state.
  void ResetContents();

  // Create the (inset relative to |box|) container for dialog elements.
  GtkWidget* CreateSubContents(GtkWidget* box);

  // Add a close button to dialog contents
  void AddCloseButton(GtkWidget* containingBox);

  // Add title to dialog contents
  void AddTitle(GtkWidget* containingBox);

  // Update the installed service buttons from |model_|.
  void UpdateInstalledServices();

  // Update the Chrome Web Store label from |model_|.
  void UpdateCWSLabel();

  // Update the suggested extension table from |model_|.
  void UpdateSuggestedExtensions();

  // Enables/disables all service buttons and extension suggestions.
  void SetWidgetsEnabled(bool enabled);

  // Adds a throbber to the extension at |index|. Returns the alignment widget
  // containing the throbber.
  GtkWidget* AddThrobberToExtensionAt(size_t index);

  // Removes the added throbber.
  void RemoveThrobber();

  // A weak pointer to the tab contents on which to display the picker UI.
  content::WebContents* web_contents_;

  // A weak pointer to the WebIntentPickerDelegate to notify when the user
  // chooses a service or cancels.
  WebIntentPickerDelegate* delegate_;

  // A weak pointer to the picker model.
  WebIntentPickerModel* model_;

  // A weak pointer to the widget that contains all other widgets in
  // the picker.
  GtkWidget* contents_;

  // A weak pointer to the header label.
  GtkWidget* header_label_;

  // The text displayed in the header label.
  string16 header_label_text_;

  // A weak pointer to the vbox that contains the buttons used to choose the
  // service.
  GtkWidget* button_vbox_;

  // A weak pointer to the Chrome Web Store header label.
  GtkWidget* cws_label_;

  // A weak pointer to the suggested extensions vbox.
  GtkWidget* extensions_vbox_;

  // This widget holds the header when showing an inline intent handler.
  GtkWidget* service_hbox_;

  // A button to close the picker.
  scoped_ptr<CustomDrawButton> close_button_;

  // The throbber to display when installing an extension.
  scoped_ptr<ThrobberGtk> throbber_;

  scoped_ptr<WaitingDialog> waiting_dialog_;

  // A weak pointer to the constrained window.
  ConstrainedWindowGtk* window_;

  // The WebContents being displayed in inline disposition.
  scoped_ptr<content::WebContents> inline_disposition_web_contents_;

  // content::WebContentsDelegate for the inline disposition dialog.
  scoped_ptr<WebIntentInlineDispositionDelegate> inline_disposition_delegate_;

  content::NotificationRegistrar registrar_;

  // Used to connect to signals fired on the host tab contents (only used while
  // showing an inline intent handler).
  scoped_ptr<ui::GtkSignalRegistrar> host_signals_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_WEB_INTENT_PICKER_GTK_H_
