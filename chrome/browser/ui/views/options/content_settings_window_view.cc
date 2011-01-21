// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/content_settings_window_view.h"

#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/options/advanced_page_view.h"
#include "chrome/browser/ui/views/options/content_filter_page_view.h"
#include "chrome/browser/ui/views/options/cookie_filter_page_view.h"
#include "chrome/browser/ui/views/options/general_page_view.h"
#include "chrome/browser/ui/views/options/plugin_filter_page_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/label.h"
#include "views/widget/root_view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

static ContentSettingsWindowView* instance_ = NULL;
// Content setting dialog bounds padding.
const int kDialogPadding = 7;

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowContentSettingsWindow(gfx::NativeWindow parent_window,
                               ContentSettingsType content_type,
                               Profile* profile) {
  DCHECK(profile);
  // If there's already an existing options window, activate it and switch to
  // the specified page.
  // TODO(beng): note this is not multi-simultaneous-profile-safe. When we care
  //             about this case this will have to be fixed.
  if (!instance_) {
    instance_ = new ContentSettingsWindowView(profile);
    views::Window::CreateChromeWindow(parent_window, gfx::Rect(), instance_);
  }
  instance_->ShowContentSettingsTab(content_type);
}

}  // namespace browser

ContentSettingsWindowView::ContentSettingsWindowView(Profile* profile)
    // Always show preferences for the original profile. Most state when off
    // the record comes from the original profile, but we explicitly use
    // the original profile to avoid potential problems.
    : profile_(profile->GetOriginalProfile()),
      label_(NULL),
      listbox_(NULL),
      current_page_(0) {
  // We don't need to observe changes in this value.
  last_selected_page_.Init(prefs::kContentSettingsWindowLastTabIndex,
                           profile->GetPrefs(), NULL);
}

ContentSettingsWindowView::~ContentSettingsWindowView() {
  STLDeleteElements(&pages_);
}

void ContentSettingsWindowView::ShowContentSettingsTab(
    ContentSettingsType page) {
  // This will show invisible windows and bring visible windows to the front.
  window()->Show();

  if (page == CONTENT_SETTINGS_TYPE_DEFAULT) {
    // Remember the last visited page from local state.
    page = static_cast<ContentSettingsType>(last_selected_page_.GetValue());
    if (page == CONTENT_SETTINGS_TYPE_DEFAULT)
      page = CONTENT_SETTINGS_TYPE_COOKIES;
  }
  // If the page number is out of bounds, reset to the first tab.
  if (page < 0 || page >= listbox_->GetRowCount())
    page = CONTENT_SETTINGS_TYPE_COOKIES;

  listbox_->SelectRow(static_cast<int>(page));
  ShowSettingsPage(listbox_->SelectedRow());
}

///////////////////////////////////////////////////////////////////////////////
// ContentSettingsWindowView, views::DialogDelegate implementation:

std::wstring ContentSettingsWindowView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CONTENT_SETTINGS_TITLE));
}

void ContentSettingsWindowView::WindowClosing() {
  instance_ = NULL;
}

bool ContentSettingsWindowView::Cancel() {
  return GetCurrentContentSettingsTabView()->CanClose();
}

views::View* ContentSettingsWindowView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// ContentSettingsWindowView, views::Listbox::Listener implementation:

void ContentSettingsWindowView::ListboxSelectionChanged(
    views::Listbox* sender) {
  DCHECK_EQ(listbox_, sender);
  ShowSettingsPage(listbox_->SelectedRow());
  last_selected_page_.SetValue(current_page_);
}

///////////////////////////////////////////////////////////////////////////////
// ContentSettingsWindowView, views::View overrides:

void ContentSettingsWindowView::Layout() {
  int listbox_width = views::Window::GetLocalizedContentsWidth(
      IDS_CONTENT_SETTINGS_DIALOG_LISTBOX_WIDTH_CHARS);
  label_->SetBounds(kDialogPadding,
                    kDialogPadding,
                    listbox_width,
                    label_->GetPreferredSize().height());

  listbox_->SetBounds(kDialogPadding,
                      2 * kDialogPadding + label_->height(),
                      listbox_width,
                      height() - (3 * kDialogPadding) - label_->height());

  if (pages_[current_page_]->GetParent()) {
    pages_[current_page_]->SetBounds(
        2 * kDialogPadding + listbox_width,
        2 * kDialogPadding + label_->height(),
        width() - (3 * kDialogPadding) - listbox_width,
        height() - (2 * kDialogPadding));
  }
}

gfx::Size ContentSettingsWindowView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_CONTENT_SETTINGS_DIALOG_WIDTH_CHARS,
      IDS_CONTENT_SETTINGS_DIALOG_HEIGHT_LINES));
}

void ContentSettingsWindowView::ViewHierarchyChanged(bool is_add,
                                                     views::View* parent,
                                                     views::View* child) {
  // Can't init before we're inserted into a Container, because we require a
  // HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// ContentSettingsWindowView, private:

void ContentSettingsWindowView::Init() {
  // Make sure we don't leak memory by calling this twice.
  DCHECK(!listbox_);

  label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_CONTENT_SETTINGS_FEATURES_LABEL));
  label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_);

  pages_.push_back(new CookieFilterPageView(profile_));
  pages_.push_back(
      new ContentFilterPageView(profile_, CONTENT_SETTINGS_TYPE_IMAGES));
  pages_.push_back(
      new ContentFilterPageView(profile_, CONTENT_SETTINGS_TYPE_JAVASCRIPT));
  pages_.push_back(new PluginFilterPageView(profile_));
  pages_.push_back(
      new ContentFilterPageView(profile_, CONTENT_SETTINGS_TYPE_POPUPS));
  pages_.push_back(
      new ContentFilterPageView(profile_, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  pages_.push_back(
      new ContentFilterPageView(profile_, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  for (size_t i = 0; i < pages_.size(); ++i) {
    pages_[i]->set_parent_owned(false);
  }
  DCHECK_EQ(static_cast<int>(pages_.size()), CONTENT_SETTINGS_NUM_TYPES);

  std::vector<string16> strings;
  strings.push_back(l10n_util::GetStringUTF16(IDS_COOKIES_TAB_LABEL));
  strings.push_back(l10n_util::GetStringUTF16(IDS_IMAGES_TAB_LABEL));
  strings.push_back(l10n_util::GetStringUTF16(IDS_JAVASCRIPT_TAB_LABEL));
  strings.push_back(l10n_util::GetStringUTF16(IDS_PLUGIN_TAB_LABEL));
  strings.push_back(l10n_util::GetStringUTF16(IDS_POPUP_TAB_LABEL));
  strings.push_back(l10n_util::GetStringUTF16(IDS_GEOLOCATION_TAB_LABEL));
  strings.push_back(l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_TAB_LABEL));
  listbox_ = new views::Listbox(strings, this);
  AddChildView(listbox_);
  CHECK_EQ(strings.size(), pages_.size());
}

void ContentSettingsWindowView::ShowSettingsPage(int page) {
  if (pages_[current_page_]->GetParent())
    RemoveChildView(pages_[current_page_]);
  current_page_ = page;
  AddChildView(pages_[current_page_]);
  Layout();
  SchedulePaint();
}

const OptionsPageView*
    ContentSettingsWindowView::GetCurrentContentSettingsTabView() const {
  return static_cast<OptionsPageView*>(pages_[current_page_]);
}
