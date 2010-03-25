// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/content_settings_window_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/options/advanced_page_view.h"
#include "chrome/browser/views/options/content_filter_page_view.h"
#include "chrome/browser/views/options/cookie_filter_page_view.h"
#include "chrome/browser/views/options/general_page_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/widget/root_view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

static ContentSettingsWindowView* instance_ = NULL;
// Content setting dialog bounds padding.
static const int kDialogPadding = 7;

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
    : tabs_(NULL),
      profile_(profile->GetOriginalProfile()) {
  // We don't need to observe changes in this value.
  last_selected_page_.Init(prefs::kContentSettingsWindowLastTabIndex,
                           profile->GetPrefs(), NULL);
}

ContentSettingsWindowView::~ContentSettingsWindowView() {
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
  if (page < 0 || page >= tabs_->GetTabCount())
    page = CONTENT_SETTINGS_TYPE_COOKIES;

  tabs_->SelectTabAt(static_cast<int>(page));
}

///////////////////////////////////////////////////////////////////////////////
// ContentSettingsWindowView, views::DialogDelegate implementation:

std::wstring ContentSettingsWindowView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_CONTENT_SETTINGS_TITLE);
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
// ContentSettingsWindowView, views::TabbedPane::Listener implementation:

void ContentSettingsWindowView::TabSelectedAt(int index) {
  last_selected_page_.SetValue(index);
}

///////////////////////////////////////////////////////////////////////////////
// ContentSettingsWindowView, views::View overrides:

void ContentSettingsWindowView::Layout() {
  tabs_->SetBounds(kDialogPadding, kDialogPadding,
                   width() - (2 * kDialogPadding),
                   height() - (2 * kDialogPadding));
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
  DCHECK(!tabs_);
  tabs_ = new views::TabbedPane;
  tabs_->SetListener(this);
  AddChildView(tabs_);
  int tab_index = 0;

  CookieFilterPageView* cookie_page = new CookieFilterPageView(profile_);
  tabs_->AddTabAtIndex(tab_index++,
                       l10n_util::GetString(IDS_COOKIES_TAB_LABEL),
                       cookie_page, false);

  ContentFilterPageView* image_page =
      new ContentFilterPageView(profile_, CONTENT_SETTINGS_TYPE_IMAGES);
  tabs_->AddTabAtIndex(tab_index++,
                       l10n_util::GetString(IDS_IMAGES_TAB_LABEL),
                       image_page, false);

  ContentFilterPageView* javascript_page =
      new ContentFilterPageView(profile_, CONTENT_SETTINGS_TYPE_JAVASCRIPT);
  tabs_->AddTabAtIndex(tab_index++,
                       l10n_util::GetString(IDS_JAVASCRIPT_TAB_LABEL),
                       javascript_page, false);

  ContentFilterPageView* plugin_page =
      new ContentFilterPageView(profile_, CONTENT_SETTINGS_TYPE_PLUGINS);
  tabs_->AddTabAtIndex(tab_index++,
                       l10n_util::GetString(IDS_PLUGIN_TAB_LABEL),
                       plugin_page, false);

  ContentFilterPageView* popup_page =
      new ContentFilterPageView(profile_, CONTENT_SETTINGS_TYPE_POPUPS);
  tabs_->AddTabAtIndex(tab_index++,
                       l10n_util::GetString(IDS_POPUP_TAB_LABEL),
                       popup_page, false);

  // TODO(joth): remove -1 when geolocation tab is added.
  DCHECK_EQ(tabs_->GetTabCount(), CONTENT_SETTINGS_NUM_TYPES - 1);
}

const OptionsPageView*
    ContentSettingsWindowView::GetCurrentContentSettingsTabView() const {
  return static_cast<OptionsPageView*>(tabs_->GetSelectedTab());
}
