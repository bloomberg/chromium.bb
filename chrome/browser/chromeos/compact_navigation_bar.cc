// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/compact_navigation_bar.h"

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/logging.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/back_forward_menu_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/status_area_view.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/theme_background.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/button_dropdown.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/controls/native/native_view_host.h"

namespace chromeos {

// Padding inside each button around the image.
static const int kInnerPadding = 1;

// Spacing between buttons.
static const int kHorizPadding = 3;

static const int kURLWidth = 180;

// Preferred height.
static const int kPreferredHeight = 25;

// Draw this much white around the URL bar to make it look larger than it
// actually is.
static const int kURLPadding = 2;

////////////////////////////////////////////////////////////////////////////////
// CompactNavigationBar public:

CompactNavigationBar::CompactNavigationBar(BrowserView* browser_view)
    : browser_view_(browser_view),
      initialized_(false) {
  SetFocusable(true);
}

CompactNavigationBar::~CompactNavigationBar() {
  if (location_entry_view_->native_view())
    location_entry_view_->Detach();
}

void CompactNavigationBar::Init() {
  DCHECK(!initialized_);
  initialized_ = true;
  Browser* browser = browser_view_->browser();
  browser->command_updater()->AddCommandObserver(IDC_BACK, this);
  browser->command_updater()->AddCommandObserver(IDC_FORWARD, this);

  back_menu_model_.reset(new BackForwardMenuModel(
      browser, BackForwardMenuModel::BACKWARD_MENU));
  forward_menu_model_.reset(new BackForwardMenuModel(
      browser, BackForwardMenuModel::FORWARD_MENU));

  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();

  back_ = new views::ButtonDropDown(this, back_menu_model_.get());
  back_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                     views::Event::EF_MIDDLE_BUTTON_DOWN);
  back_->set_tag(IDC_BACK);
  back_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_BACK));
  back_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_BACK));
  back_->SetImage(views::CustomButton::BS_NORMAL,
                  resource_bundle.GetBitmapNamed(IDR_COMPACTNAV_BACK));
  back_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                           views::ImageButton::ALIGN_MIDDLE);

  AddChildView(back_);

  bf_separator_ = new views::ImageView;
  bf_separator_->SetImage(
      resource_bundle.GetBitmapNamed(IDR_COMPACTNAV_SEPARATOR));
  AddChildView(bf_separator_);

  forward_ = new views::ButtonDropDown(this, forward_menu_model_.get());
  forward_->set_triggerable_event_flags(views::Event::EF_LEFT_BUTTON_DOWN |
                                        views::Event::EF_MIDDLE_BUTTON_DOWN);
  forward_->set_tag(IDC_FORWARD);
  forward_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_FORWARD));
  forward_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_FORWARD));
  forward_->SetImage(views::CustomButton::BS_NORMAL,
                     resource_bundle.GetBitmapNamed(IDR_COMPACTNAV_FORWARD));
  forward_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                              views::ImageButton::ALIGN_MIDDLE);
  AddChildView(forward_);

  // URL bar construction.
  location_entry_.reset(new AutocompleteEditViewGtk(
      this, browser->toolbar_model(), browser->profile(),
      browser->command_updater(), false, this));
  location_entry_->Init();
  gtk_widget_show_all(location_entry_->widget());
  gtk_widget_hide(location_entry_->widget());

  location_entry_view_ = new views::NativeViewHost;
  AddChildView(location_entry_view_);
  location_entry_view_->set_focus_view(this);
  location_entry_view_->Attach(location_entry_->widget());

  set_background(new ThemeBackground(browser_view_));
}

void CompactNavigationBar::Focus() {
  location_entry_->SetFocus();
}

void CompactNavigationBar::FocusLocation() {
  location_entry_->SetFocus();
  location_entry_->SelectAll(true);
}

gfx::Size CompactNavigationBar::GetPreferredSize() {
  int width = 0;

  width += kURLWidth + kHorizPadding + kURLPadding * 2;  // URL bar.
  width += back_->GetPreferredSize().width() + kHorizPadding +
      kInnerPadding * 2;
  width += bf_separator_->GetPreferredSize().width() + kHorizPadding;
  width += forward_->GetPreferredSize().width() + kHorizPadding +
      kInnerPadding * 2;

  width++;
  return gfx::Size(width, kPreferredHeight);
}

void CompactNavigationBar::Layout() {
  if (!initialized_)
    return;

  // We hide navigation buttons when the entry has focus.  Navigation
  // buttons' visibility is controlled in OnKillFocus/OnSetFocus methods.
  if (!back_->IsVisible()) {
    // Fill the view with the entry view while it has focus.
    location_entry_view_->SetBounds(kURLPadding, 0,
                                    width() - kHorizPadding, height());
  } else {
    // Layout forward/back buttons after entry views as follows:
    // [Entry View] [Back]|[Forward]
    int curx = 0;
    // URL bar.
    location_entry_view_->SetBounds(curx + kURLPadding, 0,
                                    kURLWidth + kURLPadding * 2, height());
    curx += kURLWidth + kHorizPadding + kURLPadding * 2;

    // "Back | Forward" section.
    gfx::Size button_size = back_->GetPreferredSize();
    button_size.set_width(button_size.width() + kInnerPadding * 2);
    back_->SetBounds(curx, 0, button_size.width(), height());
    curx += button_size.width() + kHorizPadding;

    button_size = bf_separator_->GetPreferredSize();
    bf_separator_->SetBounds(curx, 0, button_size.width(), height());
    curx += button_size.width() + kHorizPadding;

    button_size = forward_->GetPreferredSize();
    button_size.set_width(button_size.width() + kInnerPadding * 2);
    forward_->SetBounds(curx, 0, button_size.width(), height());
    curx += button_size.width() + kHorizPadding;
  }
}

void CompactNavigationBar::Paint(gfx::Canvas* canvas) {
  PaintBackground(canvas);

  // Draw a white box around the edit field so that it looks larger. This is
  // kind of what the default GTK location bar does, although they have a
  // fancier border.
  canvas->FillRectInt(0xFFFFFFFF,
                      location_entry_view_->x() - kURLPadding,
                      2,
                      location_entry_view_->width() + kURLPadding * 2,
                      height() - 5);
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation.

void CompactNavigationBar::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  int id = sender->tag();
  switch (id) {
    case IDC_BACK:
    case IDC_FORWARD:
    case IDC_RELOAD:
      // Forcibly reset the location bar, since otherwise it won't discard any
      // ongoing user edits, since it doesn't realize this is a user-initiated
      // action.
      location_entry_->RevertAll();
      break;
  }
  browser_view_->browser()->ExecuteCommandWithDisposition(
      id, event_utils::DispositionFromEventFlags(sender->mouse_event_flags()));
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteController implementation.

void CompactNavigationBar::OnAutocompleteAccept(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition,
    const GURL& alternate_nav_url) {
  AddTabWithURL(url, transition);
}

void CompactNavigationBar::OnChanged() {
  // Other one does "DoLayout" here.
}

void CompactNavigationBar::OnInputInProgress(bool in_progress) {
}

void CompactNavigationBar::OnKillFocus() {
  back_->SetVisible(true);
  bf_separator_->SetVisible(true);
  forward_->SetVisible(true);
  Layout();
  SchedulePaint();
}

void CompactNavigationBar::OnSetFocus() {
  back_->SetVisible(false);
  bf_separator_->SetVisible(false);
  forward_->SetVisible(false);
  Layout();
  SchedulePaint();
}

SkBitmap CompactNavigationBar::GetFavIcon() const {
  return SkBitmap();
}

std::wstring CompactNavigationBar::GetTitle() const {
  return std::wstring();
}

////////////////////////////////////////////////////////////////////////////////
// BubblePositioner implementation.

gfx::Rect CompactNavigationBar::GetLocationStackBounds() const {
  gfx::Point origin;
  ConvertPointToScreen(this, &origin);
  return gfx::Rect(origin, size());
}

////////////////////////////////////////////////////////////////////////////////
// CommandUpdater::CommandObserver implementation.
void CompactNavigationBar::EnabledStateChangedForCommand(int id, bool enabled) {
  switch (id) {
    case IDC_BACK:
      back_->SetEnabled(enabled);
      break;
    case IDC_FORWARD:
      forward_->SetEnabled(enabled);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// CompactNavigationBar private:

void CompactNavigationBar::AddTabWithURL(const GURL& url,
                                         PageTransition::Type transition) {
  Browser* browser = browser_view_->browser();
  switch (StatusAreaView::GetOpenTabsMode()) {
    case StatusAreaView::OPEN_TABS_ON_LEFT: {
      // Add the new tab at the first non-pinned location.
      int index = browser->tabstrip_model()->IndexOfFirstNonAppTab();
      browser->AddTabWithURL(url, GURL(), transition,
                              true, index, true, NULL);
      break;
    }
    case StatusAreaView::OPEN_TABS_CLOBBER: {
      browser->GetSelectedTabContents()->controller().LoadURL(
          url, GURL(), transition);
      break;
    }
    case StatusAreaView::OPEN_TABS_ON_RIGHT: {
      browser->AddTabWithURL(url, GURL(), transition, true, -1, true, NULL);
      break;
    }
  }
}

}  // namespace chromeos
