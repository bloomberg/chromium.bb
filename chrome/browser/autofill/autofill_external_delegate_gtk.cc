// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_external_delegate_gtk.h"

#include "chrome/browser/ui/gtk/autofill/autofill_popup_view_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

AutofillExternalDelegate* AutofillExternalDelegate::Create(
    TabContentsWrapper* tab_contents_wrapper,
    AutofillManager* autofill_manager) {
  return new AutofillExternalDelegateGtk(tab_contents_wrapper,
                                         autofill_manager);
}

AutofillExternalDelegateGtk::AutofillExternalDelegateGtk(
    TabContentsWrapper* tab_contents_wrapper,
    AutofillManager* autofill_manager)
    : AutofillExternalDelegate(tab_contents_wrapper, autofill_manager),
      web_contents_(tab_contents_wrapper->web_contents()),
      event_handler_id_(0) {
  tab_native_view_ = web_contents_->GetView()->GetNativeView();
}

AutofillExternalDelegateGtk::~AutofillExternalDelegateGtk() {
}

void AutofillExternalDelegateGtk::HideAutofillPopupInternal() {
  if (!view_.get())
    return;

  view_->Hide();
  view_.reset();

  GtkWidget* toplevel = gtk_widget_get_toplevel(tab_native_view_);
  g_signal_handler_disconnect(toplevel, event_handler_id_);
}

void AutofillExternalDelegateGtk::OnQueryPlatformSpecific(
    int query_id,
    const webkit::forms::FormData& form,
    const webkit::forms::FormField& field,
    const gfx::Rect& bounds) {
  CreateViewIfNeeded();
  view_->set_element_bounds(bounds);
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

void AutofillExternalDelegateGtk::SetBounds(const gfx::Rect& bounds) {
  CreateViewIfNeeded();
  view_->set_element_bounds(bounds);
}

void AutofillExternalDelegateGtk::CreateViewIfNeeded() {
  if (view_.get())
    return;

  view_.reset(new AutofillPopupViewGtk(web_contents_,
                                       GtkThemeService::GetFrom(profile()),
                                       this,
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
  HideAutofillPopup();

  return TRUE;
}
