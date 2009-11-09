// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/compact_location_bar.h"

#include <gtk/gtk.h>
#include <algorithm>

#include "app/l10n_util.h"
#include "base/gfx/point.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/tabs/tab.h"
#include "chrome/browser/views/tabs/tab_strip.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/widget.h"

namespace chromeos {

const int kDefaultLocationBarWidth = 300;
const int kHideTimeoutInSeconds = 2;

CompactLocationBar::CompactLocationBar(BrowserView* browser_view)
    : browser_view_(browser_view),
      current_contents_(NULL),
      reload_(NULL),
      popup_(NULL) {
  popup_timer_.reset(new base::OneShotTimer<CompactLocationBar>());
  set_background(views::Background::CreateStandardPanelBackground());
}

CompactLocationBar::~CompactLocationBar() {
  if (popup_) {
    // This is a hack to avoid deleting this twice.
    // This problem will be gone once we eliminate a popup.
    GetParent()->RemoveAllChildViews(false);
    popup_->Close();
    popup_ = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBar public:

void CompactLocationBar::StartPopupTimer() {
  if (popup_ == NULL || !popup_->IsVisible())
    return;
  if (popup_timer_->IsRunning()) {
    // Restart the timer.
    popup_timer_->Reset();
  } else {
    popup_timer_->Start(base::TimeDelta::FromSeconds(kHideTimeoutInSeconds),
                        this, &CompactLocationBar::HidePopup);
  }
}

gfx::Rect CompactLocationBar::GetBoundsUnderTab(const Tab* tab) const {
  // Get the position of the left-bottom corner of the tab on the screen
  gfx::Point tab_left_bottom(0, tab->height());
  views::View::ConvertPointToScreen(tab, &tab_left_bottom);
  // Get the position of the left edge of the window.
  gfx::Point browser_left_top(0, 0);
  views::View::ConvertPointToScreen(browser_view_,
                                    &browser_left_top);

  // The compact location bar must be smaller than browser_width.
  int width = std::min(browser_view_->width(), kDefaultLocationBarWidth);

  // Try to center around the tab, or align to the left of the window.
  // TODO(oshima): handle RTL
  int x = std::max(tab_left_bottom.x() - ((width - tab->width()) / 2),
                   browser_left_top.x());
  return gfx::Rect(x, tab_left_bottom.y(), width, 28);
}

void CompactLocationBar::UpdateBounds(const Tab* tab) {
  if (popup_ != NULL)
    popup_->SetBounds(GetBoundsUnderTab(tab));
}

void CompactLocationBar::Update(const Tab* tab, const TabContents* contents) {
  DCHECK(tab != NULL && contents != NULL);
  if (current_contents_ == contents) {
    StartPopupTimer();
    return;
  }
  CancelPopupTimer();
  HidePopup();

  if (!popup_) {
    popup_ = views::Widget::CreatePopupWidget(
        views::Widget::Transparent,
        views::Widget::AcceptEvents,
        views::Widget::DeleteOnDestroy);
    popup_->Init(NULL, GetBoundsUnderTab(tab));
    popup_->SetContentsView(this);
  } else {
    UpdateBounds(tab);
  }
  current_contents_ = contents;
  location_entry_->Update(contents);
  popup_->Show();
  // Set focus to the location entry.
  location_entry_->SetFocus();

  // Start popup timer.
  StartPopupTimer();
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBar private:

Browser* CompactLocationBar::browser() const {
  return browser_view_->browser();
}

void CompactLocationBar::CancelPopupTimer() {
  popup_timer_->Stop();
}

void CompactLocationBar::HidePopup() {
  current_contents_ = NULL;
  if (popup_) {
    CancelPopupTimer();
    popup_->Hide();
  }
}

void CompactLocationBar::Init() {
  ThemeProvider* tp = browser()->profile()->GetThemeProvider();
  SkColor color = tp->GetColor(BrowserThemeProvider::COLOR_BUTTON_BACKGROUND);
  SkBitmap* background = tp->GetBitmapNamed(IDR_THEME_BUTTON_BACKGROUND);

  // Reload button.
  reload_ = new views::ImageButton(this);
  reload_->set_tag(IDC_RELOAD);
  reload_->SetTooltipText(l10n_util::GetString(IDS_TOOLTIP_RELOAD));
  reload_->SetAccessibleName(l10n_util::GetString(IDS_ACCNAME_RELOAD));
  reload_->SetID(VIEW_ID_RELOAD_BUTTON);

  reload_->SetImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_RELOAD));
  reload_->SetImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_RELOAD_H));
  reload_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_RELOAD_P));
  reload_->SetBackground(color, background,
      tp->GetBitmapNamed(IDR_BUTTON_MASK));

  AddChildView(reload_);

  // Location bar.
  location_entry_.reset(new AutocompleteEditViewGtk(
      this, browser()->toolbar_model(), browser()->profile(),
      browser()->command_updater(), false, this));


  location_entry_->Init();
  location_entry_->Update(browser()->GetSelectedTabContents());
  gtk_widget_show_all(location_entry_->widget());
  gtk_widget_hide(location_entry_->widget());

  location_entry_view_ = new views::NativeViewHost;
  AddChildView(location_entry_view_);
  location_entry_view_->set_focus_view(this);
  location_entry_view_->Attach(location_entry_->widget());

  // TODO(oshima): Add Star Button
  location_entry_->Update(browser()->GetSelectedTabContents());
}

////////////////////////////////////////////////////////////////////////////////
// views::View overrides:

gfx::Size CompactLocationBar::GetPreferredSize() {
  if (!reload_)
    return gfx::Size();  // Not initialized yet, do nothing.

  gfx::Size sz = reload_->GetPreferredSize();

  return gfx::Size(500, sz.height());
}

void CompactLocationBar::Layout() {
  if (!reload_)
    return;  // Not initialized yet, do nothing.

  int cur_x = 0;

  gfx::Size sz = reload_->GetPreferredSize();
  reload_->SetBounds(cur_x, 0, sz.width(), sz.height());
  cur_x += sz.width();

  cur_x += 2;

  // The location bar gets the rest of the space in the middle.
  location_entry_view_->SetBounds(cur_x, 0, width() - cur_x * 2 - 2, height());

  cur_x = width() - sz.width();
}

void CompactLocationBar::Paint(gfx::Canvas* canvas) {
  View::Paint(canvas);
}

void CompactLocationBar::ViewHierarchyChanged(bool is_add, View* parent,
                                              View* child) {
  if (is_add && child == this)
    Init();
}

void CompactLocationBar::OnMouseEntered(const views::MouseEvent& event) {
  CancelPopupTimer();
}

void CompactLocationBar::OnMouseExited(const views::MouseEvent& event) {
  StartPopupTimer();
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:

void CompactLocationBar::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  int id = sender->tag();
  browser()->ExecuteCommandWithDisposition(
      id, event_utils::DispositionFromEventFlags(sender->mouse_event_flags()));
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteEditController overrides:

void CompactLocationBar::OnAutocompleteAccept(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition,
    const GURL& alternate_nav_url) {
  browser()->OpenURL(url, GURL(), disposition, transition);
}

void CompactLocationBar::OnChanged() {
  // Other one does "DoLayout" here.
}

void CompactLocationBar::OnInputInProgress(bool in_progress) {
}

SkBitmap CompactLocationBar::GetFavIcon() const {
  return SkBitmap();
}

std::wstring CompactLocationBar::GetTitle() const {
  return std::wstring();
}

////////////////////////////////////////////////////////////////////////////////
// BubblePositioner overrides:

gfx::Rect CompactLocationBar::GetLocationStackBounds() const {
  gfx::Point lower_left(0, height());
  ConvertPointToScreen(this, &lower_left);
  return gfx::Rect(lower_left.x(), lower_left.y(), 700, 100);
}

}  // namespace chromeos
