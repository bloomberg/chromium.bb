// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/compact_location_bar_view.h"

#include <gtk/gtk.h>
#include <algorithm>

#include "app/l10n_util.h"
#include "app/drag_drop_types.h"
#include "base/gfx/point.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/bookmarks/bookmark_drag_data.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view_gtk.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/chromeos/compact_location_bar_host.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/event_utils.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/toolbar_star_toggle.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/native/native_view_host.h"
#include "views/drag_utils.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace chromeos {

const int kCompactLocationBarDefaultWidth = 700;
const int kWidgetsSeparatorWidth = 2;

CompactLocationBarView::CompactLocationBarView(CompactLocationBarHost* host)
    : DropdownBarView(host),
      reload_(NULL),
      star_(NULL) {
  set_background(views::Background::CreateStandardPanelBackground());
  SetFocusable(true);
}

CompactLocationBarView::~CompactLocationBarView() {
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarView public:

void CompactLocationBarView::SetFocusAndSelection() {
  location_entry_->SetFocus();
  location_entry_->SelectAll(true);
}

void CompactLocationBarView::Update(const TabContents* contents) {
  location_entry_->Update(contents);
}


////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarView private:

Browser* CompactLocationBarView::browser() const {
  return host()->browser_view()->browser();
}

void CompactLocationBarView::Init() {
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

  star_ = new ToolbarStarToggle(this);
  star_->SetDragController(this);
  star_->set_profile(browser()->profile());
  star_->set_host_view(this);
  star_->set_bubble_positioner(this);
  star_->Init();
  AddChildView(star_);

  location_entry_->Update(browser()->GetSelectedTabContents());
}

////////////////////////////////////////////////////////////////////////////////
// views::View overrides:

gfx::Size CompactLocationBarView::GetPreferredSize() {
  if (!reload_)
    return gfx::Size();  // Not initialized yet, do nothing.

  gfx::Size reload_size = reload_->GetPreferredSize();
  gfx::Size star_size = star_->GetPreferredSize();
  gfx::Size location_size = location_entry_view_->GetPreferredSize();

  int max_height = std::max(reload_size.height(),
                            std::max(star_size.height(),
                                     location_size.height()));
  return gfx::Size(500, max_height);
}

void CompactLocationBarView::Layout() {
  if (!reload_)
    return;  // Not initialized yet, do nothing.

  int cur_x = 0;
  gfx::Size reload_size = reload_->GetPreferredSize();
  int reload_height = std::min(reload_size.height(), height());
  reload_->SetBounds(cur_x, (height() - reload_height) / 2,
                     reload_size.width(), reload_height);
  cur_x += reload_size.width() + kWidgetsSeparatorWidth;

  gfx::Size star_size = star_->GetPreferredSize();
  int star_height = std::min(star_size.height(), height());
  star_->SetBounds(cur_x, (height() - star_height) / 2,
                   star_size.width(), star_height);
  cur_x += star_size.width();

  // The location bar is located exactly to the right of the star button and
  // takes all the space to the right except for small margin at the end.
  int location_width = width() - cur_x - kWidgetsSeparatorWidth;
  location_entry_view_->SetBounds(cur_x, 0, location_width, height());
}

void CompactLocationBarView::Paint(gfx::Canvas* canvas) {
  View::Paint(canvas);
}

void CompactLocationBarView::ViewHierarchyChanged(bool is_add, View* parent,
                                              View* child) {
  if (is_add && child == this)
    Init();
}

void CompactLocationBarView::Focus() {
  location_entry_->SetFocus();
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:

void CompactLocationBarView::ButtonPressed(views::Button* sender,
                                       const views::Event& event) {
  int id = sender->tag();
  browser()->ExecuteCommandWithDisposition(
      id, event_utils::DispositionFromEventFlags(sender->mouse_event_flags()));
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteEditController overrides:

void CompactLocationBarView::OnAutocompleteAccept(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition,
    const GURL& alternate_nav_url) {
  browser()->OpenURL(url, GURL(), disposition, transition);
}

void CompactLocationBarView::OnChanged() {
  // Other one does "DoLayout" here.
}

void CompactLocationBarView::OnKillFocus() {
  clb_host()->Hide(true);
}

void CompactLocationBarView::OnSetFocus() {
}

void CompactLocationBarView::OnInputInProgress(bool in_progress) {
}

SkBitmap CompactLocationBarView::GetFavIcon() const {
  return SkBitmap();
}

std::wstring CompactLocationBarView::GetTitle() const {
  return std::wstring();
}

////////////////////////////////////////////////////////////////////////////////
// BubblePositioner overrides:
gfx::Rect CompactLocationBarView::GetLocationStackBounds() const {
  gfx::Point lower_left(0, height());
  ConvertPointToScreen(this, &lower_left);
  gfx::Rect popup = gfx::Rect(lower_left.x(), lower_left.y(),
                              kCompactLocationBarDefaultWidth, 0);
  return popup.AdjustToFit(GetWidget()->GetWindow()->GetBounds());
}

////////////////////////////////////////////////////////////////////////////////
// views::DragController overrides:
void CompactLocationBarView::WriteDragData(views::View* sender,
                                           int press_x,
                                           int press_y,
                                           OSExchangeData* data) {
  DCHECK(
      GetDragOperations(sender, press_x, press_y) != DragDropTypes::DRAG_NONE);

  UserMetrics::RecordAction("CompactLocationBar_DragStar",
                            browser()->profile());

  // If there is a bookmark for the URL, add the bookmark drag data for it. We
  // do this to ensure the bookmark is moved, rather than creating an new
  // bookmark.
  TabContents* tab = browser()->GetSelectedTabContents();
  if (tab) {
    Profile* profile = browser()->profile();
    if (profile && profile->GetBookmarkModel()) {
      const BookmarkNode* node = profile->GetBookmarkModel()->
          GetMostRecentlyAddedNodeForURL(tab->GetURL());
      if (node) {
        BookmarkDragData bookmark_data(node);
        bookmark_data.Write(profile, data);
      }
    }

    drag_utils::SetURLAndDragImage(tab->GetURL(),
                                   UTF16ToWideHack(tab->GetTitle()),
                                   tab->GetFavIcon(),
                                   data);
  }
}

int CompactLocationBarView::GetDragOperations(views::View* sender,
                                              int x,
                                              int y) {
  DCHECK(sender == star_);
  TabContents* tab = browser()->GetSelectedTabContents();
  if (!tab || !tab->ShouldDisplayURL() || !tab->GetURL().is_valid()) {
    return DragDropTypes::DRAG_NONE;
  }
  Profile* profile = browser()->profile();
  if (profile && profile->GetBookmarkModel() &&
      profile->GetBookmarkModel()->IsBookmarked(tab->GetURL())) {
    return DragDropTypes::DRAG_MOVE | DragDropTypes::DRAG_COPY |
           DragDropTypes::DRAG_LINK;
  }
  return DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_LINK;
}

}  // namespace chromeos
