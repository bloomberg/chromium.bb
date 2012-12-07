// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/autofill/autofill_external_delegate_gtk.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/autofill/autofill_popup_view_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

void AutofillExternalDelegate::CreateForWebContentsAndManager(
    content::WebContents* web_contents,
    AutofillManager* autofill_manager) {
  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      UserDataKey(),
      new AutofillExternalDelegateGtk(web_contents, autofill_manager));
}

AutofillExternalDelegateGtk::AutofillExternalDelegateGtk(
    content::WebContents* web_contents,
    AutofillManager* autofill_manager)
    : AutofillExternalDelegate(web_contents, autofill_manager),
      event_handler_id_(0) {
  tab_native_view_ = web_contents->GetView()->GetNativeView();
}

AutofillExternalDelegateGtk::~AutofillExternalDelegateGtk() {
}

void AutofillExternalDelegateGtk::HideAutofillPopupInternal() {
  if (!view_.get())
    return;

  view_->Hide();
  view_.reset();

  GtkWidget* toplevel = gtk_widget_get_toplevel(tab_native_view_);
  if (g_signal_handler_is_connected(toplevel, event_handler_id_))
    g_signal_handler_disconnect(toplevel, event_handler_id_);
}

void AutofillExternalDelegateGtk::ApplyAutofillSuggestions(
    const std::vector<string16>& autofill_values,
    const std::vector<string16>& autofill_labels,
    const std::vector<string16>& autofill_icons,
    const std::vector<int>& autofill_unique_ids) {
  view_->Show(autofill_values,
              autofill_labels,
              autofill_icons,
              autofill_unique_ids);
}

void AutofillExternalDelegateGtk::CreatePopupForElement(
    const gfx::Rect& element_bounds) {
  content::WebContents* contents = web_contents();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  view_.reset(new AutofillPopupViewGtk(contents,
                                       GtkThemeService::GetFrom(profile),
                                       this,
                                       element_bounds,
                                       tab_native_view_));

  GtkWidget* toplevel = gtk_widget_get_toplevel(tab_native_view_);
  if (!g_signal_handler_is_connected(toplevel, event_handler_id_)) {
    event_handler_id_ = g_signal_connect(
        toplevel,
        "focus-out-event",
        G_CALLBACK(HandleViewFocusOutThunk),
        this);
  }
}

gboolean AutofillExternalDelegateGtk::HandleViewFocusOut(GtkWidget* sender,
                                                         GdkEventFocus* event) {
  if (!popup_visible())
    return FALSE;

  HideAutofillPopup();

  return TRUE;
}
