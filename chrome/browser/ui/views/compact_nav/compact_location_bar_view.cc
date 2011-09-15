// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/compact_nav/compact_location_bar_view.h"

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

#include <algorithm>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/compact_nav/compact_location_bar_view_host.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/reload_button.h"
#include "content/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/point.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/native/native_view_host.h"
#include "views/widget/widget.h"

namespace {

const int kDefaultLocationEntryWidth = 375;
const int kCompactLocationLeftMargin = 7;
const int kCompactLocationRightMargin = 8;
const int kEntryPadding = 2;
// TODO(oshima): ToolbarView gets this from background image's height;
// Find out the right way, value for compact location bar.
const int kDefaultLocationBarHeight = 34;

}  // namespace

CompactLocationBarView::CompactLocationBarView(CompactLocationBarViewHost* host)
    : DropdownBarView(host),
      reload_button_(NULL),
      location_bar_view_(NULL),
      initialized_(false) {
  set_focusable(true);
}

CompactLocationBarView::~CompactLocationBarView() {
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarView public:

void CompactLocationBarView::SetFocusAndSelection(bool select_all) {
  location_bar_view_->FocusLocation(select_all);
}

void CompactLocationBarView::Update(const TabContents* contents) {
  location_bar_view_->Update(contents);
}

////////////////////////////////////////////////////////////////////////////////
// AccessiblePaneView overrides:

bool CompactLocationBarView::SetPaneFocus(
  int view_storage_id, views::View* initial_focus) {
  if (!AccessiblePaneView::SetPaneFocus(view_storage_id, initial_focus))
    return false;

  location_bar_view_->SetShowFocusRect(true);
  return true;
}

void CompactLocationBarView::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_TOOLBAR;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_TOOLBAR);
}

////////////////////////////////////////////////////////////////////////////////
// CompactLocationBarView private:

Browser* CompactLocationBarView::browser() const {
  return host()->browser_view()->browser();
}

void CompactLocationBarView::Init() {
  DCHECK(!initialized_);
  initialized_ = true;

  // Use a larger version of the system font.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  font_ = rb.GetFont(ResourceBundle::MediumFont);

  // Location bar.
  location_bar_view_ = new LocationBarView(
      browser(),
      browser()->toolbar_model(),
      clb_host(),
      LocationBarView::NORMAL);

  // Reload button.
  reload_button_ = new ReloadButton(location_bar_view_, browser());
  reload_button_->set_triggerable_event_flags(ui::EF_LEFT_BUTTON_DOWN |
      ui::EF_MIDDLE_BUTTON_DOWN);
  reload_button_->set_tag(IDC_RELOAD);
  reload_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_RELOAD));
  reload_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));
  reload_button_->set_id(VIEW_ID_RELOAD_BUTTON);

  ThemeProvider* tp = GetThemeProvider();
  reload_button_->SetImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_RELOAD));
  reload_button_->SetImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_RELOAD_H));
  reload_button_->SetImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_RELOAD_P));
  reload_button_->SetToggledImage(views::CustomButton::BS_NORMAL,
      tp->GetBitmapNamed(IDR_STOP));
  reload_button_->SetToggledImage(views::CustomButton::BS_HOT,
      tp->GetBitmapNamed(IDR_STOP_H));
  reload_button_->SetToggledImage(views::CustomButton::BS_PUSHED,
      tp->GetBitmapNamed(IDR_STOP_P));
  reload_button_->SetToggledImage(views::CustomButton::BS_DISABLED,
      tp->GetBitmapNamed(IDR_STOP_D));

  // Always add children in order from left to right, for accessibility.
  AddChildView(reload_button_);
  AddChildView(location_bar_view_);
  location_bar_view_->Init();

  SetDialogBorderBitmaps(rb.GetBitmapNamed(IDR_CNAV_DIALOG_LEFT),
                         rb.GetBitmapNamed(IDR_CNAV_DIALOG_MIDDLE),
                         rb.GetBitmapNamed(IDR_CNAV_DIALOG_RIGHT));
}

////////////////////////////////////////////////////////////////////////////////
// views::View overrides:

gfx::Size CompactLocationBarView::GetPreferredSize() {
  if (!reload_button_)
    return gfx::Size();  // Not initialized yet, do nothing.

  gfx::Size reload_size = reload_button_->GetPreferredSize();
  gfx::Size location_size = location_bar_view_->GetPreferredSize();
  int width = kCompactLocationLeftMargin + reload_size.width() +
      std::max(kDefaultLocationEntryWidth,
               location_bar_view_->GetPreferredSize().width()) +
      kCompactLocationRightMargin;
  return gfx::Size(width, kDefaultLocationBarHeight);
}

void CompactLocationBarView::OnPaint(gfx::Canvas* canvas) {
  // TODO(stevet): A lot of this method is copied almost directly from
  // FindBarView. Perhaps we can share it in the common parent class.
  SkPaint paint;

  gfx::Rect bounds = PaintOffsetToolbarBackground(canvas);

  // Now flip the canvas for the rest of the graphics if in RTL mode.
  canvas->Save();
  if (base::i18n::IsRTL()) {
    canvas->TranslateInt(width(), 0);
    canvas->ScaleInt(-1, 1);
  }

  PaintDialogBorder(canvas, bounds);

  PaintAnimatingEdges(canvas, bounds);

  canvas->Restore();
}

void CompactLocationBarView::Layout() {
  if (!reload_button_)
    return;  // Not initialized yet, do nothing.

  int cur_x = kCompactLocationLeftMargin;

  // Vertically center all items, basing off the reload button.
  gfx::Size reload_size = reload_button_->GetPreferredSize();
  int y = (height() - reload_size.height()) / 2;
  reload_button_->SetBounds(cur_x, y,
                     reload_size.width(), reload_size.height());
  cur_x += reload_size.width() + kEntryPadding;

  int location_view_width = width() - cur_x - kCompactLocationRightMargin;

  // The location bar gets the rest of the space in the middle. We assume it has
  // the same height as the reload button.
  location_bar_view_->SetBounds(cur_x, y, location_view_width,
                                reload_size.height());
}

void CompactLocationBarView::Paint(gfx::Canvas* canvas) {
  // This paints the background behind the reload button all the way across to
  // the end of the CLB. Without this, everything comes up transparent.
  gfx::Rect bounds = GetLocalBounds();
  ThemeProvider* tp = GetThemeProvider();
  // Now convert from screen to parent coordinates.
  gfx::Point origin(bounds.origin());
  BrowserView* browser_view = host()->browser_view();
  ConvertPointToView(NULL, browser_view, &origin);
  bounds.set_origin(origin);
  // Finally, calculate the background image tiling offset.
  origin = browser_view->OffsetPointForToolbarBackgroundImage(origin);

  canvas->TileImageInt(*tp->GetBitmapNamed(IDR_THEME_TOOLBAR),
                       origin.x(), origin.y(), 0, 0,
                       bounds.width(), bounds.height());
  View::Paint(canvas);
}

void CompactLocationBarView::ViewHierarchyChanged(bool is_add,
                                                  View* parent,
                                                  View* child) {
  if (is_add && child == this && !initialized_) {
    Init();
  }
}

void CompactLocationBarView::Focus() {
  location_bar_view_->SetFocusAndSelection(false);
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener overrides:

void CompactLocationBarView::ButtonPressed(views::Button* sender,
                                           const views::Event& event) {
  int id = sender->tag();
  int flags = sender->mouse_event_flags();
  // Shift-clicking or ctrl-clicking the reload button means we should
  // ignore any cached content.
  // TODO(avayvod): eliminate duplication of this logic in
  // CompactLocationBarView.
  if (id == IDC_RELOAD && (event.IsShiftDown() || event.IsControlDown())) {
    id = IDC_RELOAD_IGNORING_CACHE;
    // Mask off shift/ctrl so they aren't interpreted as affecting the
    // disposition below.
    flags &= ~(ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  }
  browser()->ExecuteCommandWithDisposition(
      id, event_utils::DispositionFromEventFlags(flags));
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteEditController overrides:

void CompactLocationBarView::OnAutocompleteAccept(
    const GURL& url,
    WindowOpenDisposition disposition,
    PageTransition::Type transition,
    const GURL& alternate_nav_url) {
  browser()->OpenURL(url, GURL(), disposition, transition);
  clb_host()->StartAutoHideTimer();
}

void CompactLocationBarView::OnChanged() {
  // TODO(stevet): Once we put in a location icon, we should resurrect this code
  // to update the icon.
  // location_icon_view_->SetImage(
  //     ResourceBundle::GetSharedInstance().GetBitmapNamed(
  //     location_entry_->GetIcon()));
  // location_icon_view_->ShowTooltip(!location_entry()->IsEditingOrEmpty());

  Layout();
  SchedulePaint();
}

void CompactLocationBarView::OnSelectionBoundsChanged() {
  // TODO(stevet): LocationBarView, for OS_WIN, uses SuggestedTextView here.
  // We should implement this usage eventually, if appropriate.
}

void CompactLocationBarView::OnKillFocus() {
  host()->UnregisterAccelerators();
}

void CompactLocationBarView::OnSetFocus() {
  clb_host()->CancelAutoHideTimer();
  views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return;
  }
  focus_manager->SetFocusedView(this);
  host()->RegisterAccelerators();
}

void CompactLocationBarView::OnInputInProgress(bool in_progress) {
}

SkBitmap CompactLocationBarView::GetFavicon() const {
  TabContentsWrapper* wrapper = browser()->GetSelectedTabContentsWrapper();
  return wrapper ? wrapper->favicon_tab_helper()->GetFavicon() : SkBitmap();
}

string16 CompactLocationBarView::GetTitle() const {
  const TabContentsWrapper* wrapper =
      browser()->GetSelectedTabContentsWrapper();
  return wrapper ? wrapper->tab_contents()->GetTitle() : string16();
}

InstantController* CompactLocationBarView::GetInstant() {
  // TODO(stevet): Re-enable instant for compact nav.
  // return browser()->instant();
  return NULL;
}

TabContentsWrapper* CompactLocationBarView::GetTabContentsWrapper() const {
  return browser()->GetSelectedTabContentsWrapper();
}
