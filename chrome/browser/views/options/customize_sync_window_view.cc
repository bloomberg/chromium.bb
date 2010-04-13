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
#include "views/controls/button/button.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

// static
CustomizeSyncWindowView* CustomizeSyncWindowView::instance_ = NULL;

CustomizeSyncWindowView::CustomizeSyncWindowView(Profile* profile)
    : profile_(profile),
      description_label_(NULL),
      bookmarks_check_box_(NULL),
      preferences_check_box_(NULL),
      autofill_check_box_(NULL),
      themes_check_box_(NULL) {
  DCHECK(profile);
  Init();
}

// static
void CustomizeSyncWindowView::Show(gfx::NativeWindow parent_window,
                                   Profile* profile) {
  DCHECK(profile);
  if (!instance_) {
    instance_ = new CustomizeSyncWindowView(profile);

    // |instance_| will get deleted once Close() is called.
    views::Window::CreateChromeWindow(parent_window, gfx::Rect(), instance_);
  }
  if (!instance_->window()->IsVisible()) {
    instance_->window()->Show();
  } else {
    instance_->window()->Activate();
  }
}

// static
bool CustomizeSyncWindowView::ClickOk() {
  if (instance_) {
    if (instance_->IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK)) {
      instance_->Accept();
      instance_->window()->Close();
      return true;
    } else {
      instance_->Focus();
      return false;
    }
  } else {
    return true;
  }
}

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
    last_view = autofill_check_box_;
  }

  if (themes_check_box_) {
    sz = themes_check_box_->GetPreferredSize();
    themes_check_box_->SetBounds(2 * kPanelHorizMargin,
        last_view->y() +
        last_view->height() +
        kRelatedControlVerticalSpacing,
        sz.width(), sz.height());
    last_view = themes_check_box_;
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
  syncable::ModelTypeSet desired_types;

  if (bookmarks_check_box_->checked()) {
    desired_types.insert(syncable::BOOKMARKS);
  }
  if (preferences_check_box_ && preferences_check_box_->checked()) {
    desired_types.insert(syncable::PREFERENCES);
  }
  if (autofill_check_box_ && autofill_check_box_->checked()) {
    desired_types.insert(syncable::AUTOFILL);
  }
  if (themes_check_box_ && themes_check_box_->checked()) {
    desired_types.insert(syncable::THEMES);
  }

  // You shouldn't be able to accept if you've selected 0 datatypes.
  DCHECK(!desired_types.empty());
  profile_->GetProfileSyncService()->ChangePreferredDataTypes(desired_types);

  return true;
}

int CustomizeSyncWindowView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_OK |
         MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

bool CustomizeSyncWindowView::IsDialogButtonEnabled(
  MessageBoxFlags::DialogButton button) const {
    switch (button) {
      case MessageBoxFlags::DIALOGBUTTON_OK:
        // The OK button should be enabled if any checkbox is checked.
        return bookmarks_check_box_->checked() ||
            (preferences_check_box_ && preferences_check_box_->checked()) ||
            (autofill_check_box_ && autofill_check_box_->checked()) ||
            (themes_check_box_ && themes_check_box_->checked());
      case MessageBoxFlags::DIALOGBUTTON_CANCEL:
        return true;
      default:
        NOTREACHED() << "CustomizeSyncWindowView should only have OK and "
          << "Cancel buttons.";
        return false;
    }
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
// CustomizeSyncWindowView, views::ButtonListener implementations

void CustomizeSyncWindowView::ButtonPressed(views::Button* sender,
                                            const views::Event& event) {
  GetWindow()->GetClientView()->AsDialogClientView()->UpdateDialogButtons();
}

/////////////////////////////////////////////////////////////////////////////
// CustomizeSyncWindowView, private

views::Checkbox* CustomizeSyncWindowView::AddCheckbox(const std::wstring& text,
                                                      bool checked) {
  views::Checkbox* checkbox = new views::Checkbox(text);
  checkbox->SetChecked(checked);
  checkbox->set_listener(this);
  AddChildView(checkbox);
  return checkbox;
}

void CustomizeSyncWindowView::Init() {
  syncable::ModelTypeSet registered_types;
  profile_->GetProfileSyncService()->GetRegisteredDataTypes(&registered_types);

  syncable::ModelTypeSet preferred_types;
  profile_->GetProfileSyncService()->GetPreferredDataTypes(&preferred_types);

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
  DCHECK(registered_types.count(syncable::BOOKMARKS));
  bool bookmarks_checked = preferred_types.count(syncable::BOOKMARKS) != 0;
  bookmarks_check_box_ = AddCheckbox(
      l10n_util::GetString(IDS_SYNC_DATATYPE_BOOKMARKS), bookmarks_checked);

  if (registered_types.count(syncable::PREFERENCES)) {
    bool prefs_checked = preferred_types.count(syncable::PREFERENCES) != 0;
    preferences_check_box_ = AddCheckbox(
        l10n_util::GetString(IDS_SYNC_DATATYPE_PREFERENCES), prefs_checked);
  }

  if (registered_types.count(syncable::AUTOFILL)) {
    bool autofill_checked = preferred_types.count(syncable::AUTOFILL) != 0;
    autofill_check_box_ = AddCheckbox(
        l10n_util::GetString(IDS_SYNC_DATATYPE_AUTOFILL), autofill_checked);
  }

  if (registered_types.count(syncable::THEMES)) {
    bool themes_checked = preferred_types.count(syncable::THEMES) != 0;
    themes_check_box_ = AddCheckbox(
        l10n_util::GetString(IDS_SYNC_DATATYPE_THEMES), themes_checked);
  }
}
