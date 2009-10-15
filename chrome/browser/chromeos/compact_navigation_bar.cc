// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/compact_navigation_bar.h"

#include "app/gfx/canvas.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/logging.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/status_area_view.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/controls/native/native_view_host.h"

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

CompactNavigationBar::CompactNavigationBar(Browser* browser)
    : browser_(browser),
      initialized_(false) {
}

CompactNavigationBar::~CompactNavigationBar() {
  location_entry_view_->Detach();
}

void CompactNavigationBar::Init() {
  DCHECK(!initialized_);
  initialized_ = true;

  ResourceBundle& resource_bundle = ResourceBundle::GetSharedInstance();

  back_button_ = new views::ImageButton(this);
  back_button_->SetImage(views::CustomButton::BS_NORMAL,
      resource_bundle.GetBitmapNamed(IDR_COMPACTNAV_BACK));
  back_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                  views::ImageButton::ALIGN_MIDDLE);
  AddChildView(back_button_);

  bf_separator_ = new views::ImageView;
  bf_separator_->SetImage(
      resource_bundle.GetBitmapNamed(IDR_COMPACTNAV_SEPARATOR));
  AddChildView(bf_separator_);

  forward_button_ = new views::ImageButton(this);
  forward_button_->SetImage(views::CustomButton::BS_NORMAL,
      resource_bundle.GetBitmapNamed(IDR_COMPACTNAV_FORWARD));
  forward_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                     views::ImageButton::ALIGN_MIDDLE);
  AddChildView(forward_button_);

  // URL bar construction.
  location_entry_.reset(new AutocompleteEditViewGtk(
      this, browser_->toolbar_model(), browser_->profile(),
      browser_->command_updater(), false, this));
  location_entry_->Init();
  gtk_widget_show_all(location_entry_->widget());
  gtk_widget_hide(location_entry_->widget());

  location_entry_view_ = new views::NativeViewHost;
  AddChildView(location_entry_view_);
  location_entry_view_->set_focus_view(this);
  location_entry_view_->Attach(location_entry_->widget());
}

gfx::Size CompactNavigationBar::GetPreferredSize() {
  int width = 0;

  width += kURLWidth + kHorizPadding + kURLPadding * 2;  // URL bar.
  width += back_button_->GetPreferredSize().width() + kHorizPadding +
      kInnerPadding * 2;
  width += bf_separator_->GetPreferredSize().width() + kHorizPadding;
  width += forward_button_->GetPreferredSize().width() + kHorizPadding +
      kInnerPadding * 2;

  width++;
  return gfx::Size(width, kPreferredHeight);
}

void CompactNavigationBar::Layout() {
  if (!initialized_)
    return;

  int curx = 0;

  // "Back | Forward" section.
  gfx::Size button_size = back_button_->GetPreferredSize();
  button_size.set_width(button_size.width() + kInnerPadding * 2);
  back_button_->SetBounds(curx, 0, button_size.width(), height());
  curx += button_size.width() + kHorizPadding;

  button_size = bf_separator_->GetPreferredSize();
  bf_separator_->SetBounds(curx, 0, button_size.width(), height());
  curx += button_size.width() + kHorizPadding;

  button_size = forward_button_->GetPreferredSize();
  button_size.set_width(button_size.width() + kInnerPadding * 2);
  forward_button_->SetBounds(curx, 0, button_size.width(), height());
  curx += button_size.width() + kHorizPadding;

  // URL bar.
  location_entry_view_->SetBounds(curx + kURLPadding, 0,
                                  kURLWidth + kURLPadding * 2, height());
  curx += kURLWidth + kHorizPadding + kURLPadding * 2;
}

void CompactNavigationBar::Paint(gfx::Canvas* canvas) {
  ThemeProvider* theme = browser_->profile()->GetThemeProvider();

  // Fill the background.
  int image_name;
  if (browser_->window()->IsActive()) {
    image_name = browser_->profile()->IsOffTheRecord() ?
                 IDR_THEME_FRAME_INCOGNITO : IDR_THEME_FRAME;
  } else {
    image_name = browser_->profile()->IsOffTheRecord() ?
                 IDR_THEME_FRAME_INCOGNITO_INACTIVE : IDR_THEME_FRAME_INACTIVE;
  }
  SkBitmap* background = theme->GetBitmapNamed(image_name);
  canvas->TileImageInt(*background, 0, 0, width(), height());

  // Draw a white box around the edit field so that it looks larger. This is
  // kind of what the default GTK location bar does, although they have a
  // fancier border.
  canvas->FillRectInt(0xFFFFFFFF,
                      location_entry_view_->x() - kURLPadding,
                      2,
                      location_entry_view_->width() + kURLPadding * 2,
                      height() - 5);
}

void CompactNavigationBar::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  TabContents* tab_contents = browser_->GetSelectedTabContents();
  if (!tab_contents)
    return;

  if (sender == back_button_) {
    if (tab_contents->controller().CanGoBack())
      tab_contents->controller().GoBack();
  } else if (sender == forward_button_) {
    if (tab_contents->controller().CanGoForward())
      tab_contents->controller().GoForward();
  } else {
    NOTREACHED();
  }
}

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

void CompactNavigationBar::OnSetFocus() {
}

SkBitmap CompactNavigationBar::GetFavIcon() const {
  return SkBitmap();
}

std::wstring CompactNavigationBar::GetTitle() const {
  return std::wstring();
}

gfx::Rect CompactNavigationBar::GetLocationStackBounds() const {
  gfx::Point origin;
  ConvertPointToScreen(this, &origin);
  return gfx::Rect(origin, size());
}

void CompactNavigationBar::AddTabWithURL(const GURL& url,
                                         PageTransition::Type transition) {
  switch (StatusAreaView::GetOpenTabsMode()) {
    case StatusAreaView::OPEN_TABS_ON_LEFT: {
      // Add the new tab at the first non-pinned location.
      int index = browser_->tabstrip_model()->IndexOfFirstNonPinnedTab();
      browser_->AddTabWithURL(url, GURL(), transition,
                              true, index, true, NULL);
      break;
    }
    case StatusAreaView::OPEN_TABS_CLOBBER: {
      browser_->GetSelectedTabContents()->controller().LoadURL(
          url, GURL(), transition);
      break;
    }
    case StatusAreaView::OPEN_TABS_ON_RIGHT: {
      browser_->AddTabWithURL(url, GURL(), transition, true, -1, true, NULL);
      break;
    }
  }
}
