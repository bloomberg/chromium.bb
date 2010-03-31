// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/views/options/customize_sync_window_view.h"
#include "chrome/common/pref_names.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "gfx/font.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/label.h"
#include "views/controls/button/checkbox.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

// static
CustomizeSyncWindowView* CustomizeSyncWindowView::instance_ = NULL;

CustomizeSyncWindowView::CustomizeSyncWindowView(Profile* profile,
                                                 bool configure_on_accept)
    : profile_(profile),
      configure_on_accept_(configure_on_accept),
      description_label_(NULL),
      bookmarks_check_box_(NULL),
      preferences_check_box_(NULL),
      autofill_check_box_(NULL) {
  DCHECK(profile);
  Init();
}

// static
void CustomizeSyncWindowView::Show(gfx::NativeWindow parent_window,
                                   Profile* profile,
                                   bool configure_on_accept) {
  DCHECK(profile);
  if (!instance_) {
    instance_ = new CustomizeSyncWindowView(profile, configure_on_accept);

    // |instance_| will get deleted once Close() is called.
    views::Window::CreateChromeWindow(parent_window, gfx::Rect(), instance_);
  }
  if (!instance_->window()->IsVisible()) {
    instance_->window()->Show();
  } else {
    instance_->window()->Activate();
  }

  instance_->configure_on_accept_ = configure_on_accept;
}

// static
void CustomizeSyncWindowView::ClickOk() {
  if (instance_) {
    instance_->Accept();
    instance_->window()->Close();
  }
}

// Ideally this would do the same as when you click "cancel".
// static
void CustomizeSyncWindowView::ClickCancel() {
  if (instance_) {
    instance_->Cancel();
    instance_->window()->Close();
  }
}

/////////////////////////////////////////////////////////////////////////////
// CustomizeSyncWindowView, views::View implementations

void CustomizeSyncWindowView::Layout() {
  gfx::Size sz = description_label_->GetPreferredSize();
  description_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
      sz.width(), sz.height());

  sz = bookmarks_check_box_->GetPreferredSize();
  bookmarks_check_box_->SetBounds(2 * kPanelHorizMargin,
      description_label_->y() +
      description_label_->height() +
      kRelatedControlVerticalSpacing,
      sz.width(), sz.height());

  View* last_view = bookmarks_check_box_;
  if (preferences_check_box_) {
    sz = preferences_check_box_->GetPreferredSize();
    preferences_check_box_->SetBounds(2 * kPanelHorizMargin,
        last_view->y() +
        last_view->height() +
        kRelatedControlVerticalSpacing,
        sz.width(), sz.height());
    last_view = preferences_check_box_;
  }

  if (autofill_check_box_) {
    sz = autofill_check_box_->GetPreferredSize();
    autofill_check_box_->SetBounds(2 * kPanelHorizMargin,
        last_view->y() +
        last_view->height() +
        kRelatedControlVerticalSpacing,
        sz.width(), sz.height());
  }
}

gfx::Size CustomizeSyncWindowView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_CUSTOMIZE_SYNC_DIALOG_WIDTH_CHARS,
      IDS_CUSTOMIZE_SYNC_DIALOG_HEIGHT_LINES));
}

void CustomizeSyncWindowView::ViewHierarchyChanged(
  bool is_add, views::View* parent, views::View* child) {
    if (is_add && child == this)
      Init();
}

/////////////////////////////////////////////////////////////////////////////
// CustomizeSyncWindowView, views::DialogDelegate implementations

bool CustomizeSyncWindowView::Accept() {
  browser_sync::DataTypeManager::TypeSet desired_types;

  profile_->GetPrefs()->SetBoolean(prefs::kSyncBookmarks,
      bookmarks_check_box_->checked());
  if (bookmarks_check_box_->checked()) {
    desired_types.insert(syncable::BOOKMARKS);
  }

  if (preferences_check_box_) {
    profile_->GetPrefs()->SetBoolean(prefs::kSyncPreferences,
        preferences_check_box_->checked());
    if (preferences_check_box_->checked()) {
      desired_types.insert(syncable::PREFERENCES);
    }
  }
  if (autofill_check_box_) {
    profile_->GetPrefs()->SetBoolean(prefs::kSyncAutofill,
        autofill_check_box_->checked());
    if (autofill_check_box_->checked()) {
      desired_types.insert(syncable::AUTOFILL);
    }
  }
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();

  if (configure_on_accept_) {
    profile_->GetProfileSyncService()->ChangeDataTypes(desired_types);
  }
  return true;
}

int CustomizeSyncWindowView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_OK |
         MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

std::wstring CustomizeSyncWindowView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_CUSTOMIZE_SYNC_WINDOW_TITLE);
}

views::View* CustomizeSyncWindowView::GetContentsView() {
  return this;
}

void CustomizeSyncWindowView::WindowClosing() {
  // |instance_| is deleted once the window is closed, so we just have to set
  // it to NULL.
  instance_ = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// CustomizeSyncWindowView, private

views::Checkbox* CustomizeSyncWindowView::AddCheckbox(const std::wstring& text,
                                                      bool checked) {
  views::Checkbox* checkbox = new views::Checkbox(text);
  checkbox->SetChecked(checked);
  AddChildView(checkbox);
  return checkbox;
}

void CustomizeSyncWindowView::Init() {
  browser_sync::DataTypeController::StateMap states_obj;
  browser_sync::DataTypeController::StateMap* controller_states = &states_obj;
  profile_->GetProfileSyncService()->GetDataTypeControllerStates(
      controller_states);

  description_label_ = new views::Label();
  description_label_->SetText(
      l10n_util::GetString(IDS_CUSTOMIZE_SYNC_DESCRIPTION));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  description_label_->SetFont(title_font);
  AddChildView(description_label_);

  // If the user hasn't set up sync yet, check the box (because all sync types
  // should be on by default).  If the user has, then check the box for a
  // data type if that data type is already being synced.
  if (controller_states->count(syncable::BOOKMARKS)) {
    bool bookmarks_checked =
        !configure_on_accept_ ||
        controller_states->find(syncable::BOOKMARKS)->second
        == browser_sync::DataTypeController::RUNNING;
    bookmarks_check_box_ = AddCheckbox(
        l10n_util::GetString(IDS_SYNC_DATATYPE_BOOKMARKS), bookmarks_checked);
  }

  if (controller_states->count(syncable::PREFERENCES)) {
    bool prefs_checked =
        !configure_on_accept_ ||
        controller_states->find(syncable::PREFERENCES)->second
        == browser_sync::DataTypeController::RUNNING;
    preferences_check_box_ = AddCheckbox(
        l10n_util::GetString(IDS_SYNC_DATATYPE_PREFERENCES), prefs_checked);
  }

  if (controller_states->count(syncable::AUTOFILL)) {
    bool autofill_checked =
        !configure_on_accept_ ||
        controller_states->find(syncable::AUTOFILL)->second
        == browser_sync::DataTypeController::RUNNING;
    autofill_check_box_ = AddCheckbox(
        l10n_util::GetString(IDS_SYNC_DATATYPE_AUTOFILL), autofill_checked);
  }
}
