// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/customize_sync_window_gtk.h"

#include <gtk/gtk.h>

#include <string>

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/options_window.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

///////////////////////////////////////////////////////////////////////////////
// CustomizeSyncWindowGtk
//
// The contents of the Customize Sync dialog window.

class CustomizeSyncWindowGtk {
 public:
  CustomizeSyncWindowGtk(Profile* profile, bool configure_on_accept);
  ~CustomizeSyncWindowGtk();

  void Show();
  void SetConfigureOnAccept(bool configure_on_accept) {
    configure_on_accept_ = configure_on_accept;
  }
  void ClickOk();
  void ClickCancel();

 private:
  // The pixel width we wrap labels at.
  static const int kWrapWidth = 475;

  GtkWidget* AddCheckbox(GtkWidget* parent, int label_id, bool checked);
  bool Accept();

  static void OnWindowDestroy(GtkWidget* widget,
                              CustomizeSyncWindowGtk* window);

  static void OnResponse(GtkDialog* dialog, gint response_id,
                         CustomizeSyncWindowGtk* customize_sync_window);

  // The passwords and exceptions dialog.
  GtkWidget *dialog_;

  Profile* profile_;

  bool configure_on_accept_;
  GtkWidget* autofill_check_box_;
  GtkWidget* bookmarks_check_box_;
  GtkWidget* preferences_check_box_;

  // Helper object to manage accessibility metadata.
  scoped_ptr<AccessibleWidgetHelper> accessible_widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(CustomizeSyncWindowGtk);
};

// The singleton customize sync window object.
static CustomizeSyncWindowGtk* customize_sync_window = NULL;

///////////////////////////////////////////////////////////////////////////////
// CustomizeSyncWindowGtk, public:

CustomizeSyncWindowGtk::CustomizeSyncWindowGtk(Profile* profile,
                                               bool configure_on_accept)
    : profile_(profile),
      configure_on_accept_(configure_on_accept),
      autofill_check_box_(NULL),
      bookmarks_check_box_(NULL),
      preferences_check_box_(NULL) {
  browser_sync::DataTypeController::StateMap states_obj;
  browser_sync::DataTypeController::StateMap* controller_states = &states_obj;
  profile_->GetProfileSyncService()->GetDataTypeControllerStates(
      controller_states);

  std::string dialog_name = l10n_util::GetStringUTF8(
      IDS_CUSTOMIZE_SYNC_WINDOW_TITLE);
  dialog_ = gtk_dialog_new_with_buttons(
      dialog_name.c_str(),
      // Customize sync window is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_OK,
      GTK_RESPONSE_OK,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      NULL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  accessible_widget_helper_.reset(new AccessibleWidgetHelper(
      dialog_, profile));
  accessible_widget_helper_->SendOpenWindowNotification(dialog_name);

  if (controller_states->count(syncable::BOOKMARKS)) {
    bool bookmarks_checked =
        !configure_on_accept_ ||
        controller_states->find(syncable::BOOKMARKS)->second
        == browser_sync::DataTypeController::RUNNING;
    bookmarks_check_box_ = AddCheckbox(GTK_DIALOG(dialog_)->vbox,
                                       IDS_SYNC_DATATYPE_BOOKMARKS,
                                       bookmarks_checked);
  }

  if (controller_states->count(syncable::PREFERENCES)) {
    bool prefs_checked =
        !configure_on_accept_ ||
        controller_states->find(syncable::PREFERENCES)->second
        == browser_sync::DataTypeController::RUNNING;
    preferences_check_box_ = AddCheckbox(GTK_DIALOG(dialog_)->vbox,
                                         IDS_SYNC_DATATYPE_PREFERENCES,
                                         prefs_checked);
  }

  if (controller_states->count(syncable::AUTOFILL)) {
    bool autofill_checked =
        !configure_on_accept_ ||
        controller_states->find(syncable::AUTOFILL)->second
        == browser_sync::DataTypeController::RUNNING;
    autofill_check_box_ = AddCheckbox(GTK_DIALOG(dialog_)->vbox,
                                      IDS_SYNC_DATATYPE_AUTOFILL,
                                      autofill_checked);
  }

  gtk_widget_realize(dialog_);
  gtk_util::SetWindowSizeFromResources(GTK_WINDOW(dialog_),
                                       IDS_CUSTOMIZE_SYNC_DIALOG_WIDTH_CHARS,
                                       IDS_CUSTOMIZE_SYNC_DIALOG_HEIGHT_LINES,
                                       true);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);

  gtk_widget_show_all(dialog_);
}

CustomizeSyncWindowGtk::~CustomizeSyncWindowGtk() {
}

void CustomizeSyncWindowGtk::Show() {
  // Bring options window to front if it already existed and isn't already
  // in front
  gtk_window_present(GTK_WINDOW(dialog_));
}

void CustomizeSyncWindowGtk::ClickOk() {
  Accept();
  gtk_widget_destroy(GTK_WIDGET(dialog_));
}

void CustomizeSyncWindowGtk::ClickCancel() {
  gtk_widget_destroy(GTK_WIDGET(dialog_));
}

///////////////////////////////////////////////////////////////////////////////
// CustomizeSyncWindowGtk, private:

GtkWidget* CustomizeSyncWindowGtk::AddCheckbox(GtkWidget* parent, int label_id,
                                               bool checked) {
  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(label_id).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_widget_set_size_request(label, kWrapWidth, -1);

  GtkWidget* checkbox = gtk_check_button_new();
  gtk_container_add(GTK_CONTAINER(checkbox), label);

  gtk_box_pack_start(GTK_BOX(parent), checkbox, FALSE, FALSE, 0);
  accessible_widget_helper_->SetWidgetName(checkbox, label_id);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), checked);

  return checkbox;
}

bool CustomizeSyncWindowGtk::Accept() {
  browser_sync::DataTypeManager::TypeSet desired_types;

  bool bookmarks_enabled = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(bookmarks_check_box_));
  profile_->GetPrefs()->SetBoolean(prefs::kSyncBookmarks, bookmarks_enabled);
  if (bookmarks_enabled) {
    desired_types.insert(syncable::BOOKMARKS);
  }

  if (preferences_check_box_) {
    bool preferences_enabled = gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(preferences_check_box_));
    profile_->GetPrefs()->SetBoolean(prefs::kSyncPreferences,
                                     preferences_enabled);
    if (preferences_enabled) {
      desired_types.insert(syncable::PREFERENCES);
    }
  }
  if (autofill_check_box_) {
    bool autofill_enabled = gtk_toggle_button_get_active(
        GTK_TOGGLE_BUTTON(autofill_check_box_));
    profile_->GetPrefs()->SetBoolean(prefs::kSyncAutofill, autofill_enabled);
    if (autofill_enabled) {
      desired_types.insert(syncable::AUTOFILL);
    }
  }
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  if (configure_on_accept_) {
    profile_->GetProfileSyncService()->ChangeDataTypes(desired_types);
  }
  return true;
}

// static
void CustomizeSyncWindowGtk::OnWindowDestroy(GtkWidget* widget,
                                             CustomizeSyncWindowGtk* window) {
  customize_sync_window = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, window);
}

// static
void CustomizeSyncWindowGtk::OnResponse(
    GtkDialog* dialog, gint response_id,
    CustomizeSyncWindowGtk* customize_sync_window) {
  if (response_id == GTK_RESPONSE_OK) {
    customize_sync_window->ClickOk();
  } else if (response_id == GTK_RESPONSE_CANCEL) {
    customize_sync_window->ClickCancel();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowCustomizeSyncWindow(Profile* profile, bool configure_on_accept) {
  DCHECK(profile);
  // If there's already an existing window, use it.
  if (!customize_sync_window) {
    customize_sync_window = new CustomizeSyncWindowGtk(profile,
                                                       configure_on_accept);
  }
  customize_sync_window->Show();
  customize_sync_window->SetConfigureOnAccept(configure_on_accept);
}

void CustomizeSyncWindowOk() {
  if (customize_sync_window) {
    customize_sync_window->ClickOk();
  }
}

void CustomizeSyncWindowCancel() {
  if (customize_sync_window) {
    customize_sync_window->ClickCancel();
  }
}
