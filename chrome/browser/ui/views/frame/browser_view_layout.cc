// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view_layout.h"

#include "chrome/browser/sidebar/sidebar_manager.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmark_bar_view.h"
#include "chrome/browser/ui/views/download_shelf_view.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/views/tabs/side_tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "gfx/scrollbar_size.h"
#include "gfx/point.h"
#include "gfx/size.h"
#include "views/controls/single_split_view.h"
#include "views/window/window.h"

#if defined(OS_LINUX)
#include "views/window/hit_test.h"
#endif

namespace {

// The visible height of the shadow above the tabs. Clicks in this area are
// treated as clicks to the frame, rather than clicks to the tab.
const int kTabShadowSize = 2;
// The vertical overlap between the TabStrip and the Toolbar.
const int kToolbarTabStripVerticalOverlap = 3;
// An offset distance between certain toolbars and the toolbar that preceded
// them in layout.
const int kSeparationLineHeight = 1;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, public:

BrowserViewLayout::BrowserViewLayout()
    : tabstrip_(NULL),
      toolbar_(NULL),
      contents_split_(NULL),
      contents_container_(NULL),
      infobar_container_(NULL),
      download_shelf_(NULL),
      active_bookmark_bar_(NULL),
      browser_view_(NULL),
      find_bar_y_(0) {
}

BrowserViewLayout::~BrowserViewLayout() {
}

gfx::Size BrowserViewLayout::GetMinimumSize() {
  // TODO(noname): In theory the tabstrip width should probably be
  // (OTR + tabstrip + caption buttons) width.
  gfx::Size tabstrip_size(
      browser()->SupportsWindowFeature(Browser::FEATURE_TABSTRIP) ?
      tabstrip_->GetMinimumSize() : gfx::Size());
  gfx::Size toolbar_size(
      (browser()->SupportsWindowFeature(Browser::FEATURE_TOOLBAR) ||
       browser()->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR)) ?
      toolbar_->GetMinimumSize() : gfx::Size());
  if (tabstrip_size.height() && toolbar_size.height())
    toolbar_size.Enlarge(0, -kToolbarTabStripVerticalOverlap);
  gfx::Size bookmark_bar_size;
  if (active_bookmark_bar_ &&
      browser()->SupportsWindowFeature(Browser::FEATURE_BOOKMARKBAR)) {
    bookmark_bar_size = active_bookmark_bar_->GetMinimumSize();
    bookmark_bar_size.Enlarge(0, -(kSeparationLineHeight +
        active_bookmark_bar_->GetToolbarOverlap(true)));
  }
  gfx::Size contents_size(contents_split_->GetMinimumSize());

  int min_height = tabstrip_size.height() + toolbar_size.height() +
      bookmark_bar_size.height() + contents_size.height();
  int widths[] = { tabstrip_size.width(), toolbar_size.width(),
                   bookmark_bar_size.width(), contents_size.width() };
  int min_width = *std::max_element(&widths[0], &widths[arraysize(widths)]);
  return gfx::Size(min_width, min_height);
}

gfx::Rect BrowserViewLayout::GetFindBarBoundingBox() const {
  // This function returns the area the Find Bar can be laid out
  // within. This basically implies the "user-perceived content
  // area" of the browser window excluding the vertical
  // scrollbar. This is not quite so straightforward as positioning
  // based on the TabContentsContainer since the BookmarkBarView may
  // be visible but not persistent (in the New Tab case) and we
  // position the Find Bar over the top of it in that case since the
  // BookmarkBarView is not _visually_ connected to the Toolbar.

  // First determine the bounding box of the content area in Widget
  // coordinates.
  gfx::Rect bounding_box(contents_container_->bounds());

  gfx::Point topleft;
  views::View::ConvertPointToWidget(contents_container_, &topleft);
  bounding_box.set_origin(topleft);

  // Adjust the position and size of the bounding box by the find bar offset
  // calculated during the last Layout.
  int height_delta = find_bar_y_ - bounding_box.y();
  bounding_box.set_y(find_bar_y_);
  bounding_box.set_height(std::max(0, bounding_box.height() + height_delta));

  // Finally decrease the width of the bounding box by the width of
  // the vertical scroll bar.
  int scrollbar_width = gfx::scrollbar_size();
  bounding_box.set_width(std::max(0, bounding_box.width() - scrollbar_width));
  if (base::i18n::IsRTL())
    bounding_box.set_x(bounding_box.x() + scrollbar_width);

  return bounding_box;
}

bool BrowserViewLayout::IsPositionInWindowCaption(
    const gfx::Point& point) {
  gfx::Point tabstrip_point(point);
  views::View::ConvertPointToView(browser_view_, tabstrip_, &tabstrip_point);
  return tabstrip_->IsPositionInWindowCaption(tabstrip_point);
}

int BrowserViewLayout::NonClientHitTest(
    const gfx::Point& point) {
  // Since the TabStrip only renders in some parts of the top of the window,
  // the un-obscured area is considered to be part of the non-client caption
  // area of the window. So we need to treat hit-tests in these regions as
  // hit-tests of the titlebar.

  views::View* parent = browser_view_->GetParent();

  gfx::Point point_in_browser_view_coords(point);
  views::View::ConvertPointToView(
      parent, browser_view_, &point_in_browser_view_coords);

  // Determine if the TabStrip exists and is capable of being clicked on. We
  // might be a popup window without a TabStrip.
  if (browser_view_->IsTabStripVisible()) {
    // See if the mouse pointer is within the bounds of the TabStrip.
    gfx::Point point_in_tabstrip_coords(point);
    views::View::ConvertPointToView(parent, tabstrip_,
                                    &point_in_tabstrip_coords);
    if (tabstrip_->HitTest(point_in_tabstrip_coords)) {
      if (tabstrip_->IsPositionInWindowCaption(point_in_tabstrip_coords))
        return HTCAPTION;
      return HTCLIENT;
    }

    // The top few pixels of the TabStrip are a drop-shadow - as we're pretty
    // starved of dragable area, let's give it to window dragging (this also
    // makes sense visually).
    if (!browser_view_->IsMaximized() &&
        (point_in_browser_view_coords.y() <
         (tabstrip_->y() + kTabShadowSize))) {
      // We return HTNOWHERE as this is a signal to our containing
      // NonClientView that it should figure out what the correct hit-test
      // code is given the mouse position...
      return HTNOWHERE;
    }
  }

  // If the point's y coordinate is below the top of the toolbar and otherwise
  // within the bounds of this view, the point is considered to be within the
  // client area.
  gfx::Rect bv_bounds = browser_view_->bounds();
  bv_bounds.Offset(0, toolbar_->y());
  bv_bounds.set_height(bv_bounds.height() - toolbar_->y());
  if (bv_bounds.Contains(point))
    return HTCLIENT;

  // If the point's y coordinate is above the top of the toolbar, but not in
  // the tabstrip (per previous checking in this function), then we consider it
  // in the window caption (e.g. the area to the right of the tabstrip
  // underneath the window controls). However, note that we DO NOT return
  // HTCAPTION here, because when the window is maximized the window controls
  // will fall into this space (since the BrowserView is sized to entire size
  // of the window at that point), and the HTCAPTION value will cause the
  // window controls not to work. So we return HTNOWHERE so that the caller
  // will hit-test the window controls before finally falling back to
  // HTCAPTION.
  bv_bounds = browser_view_->bounds();
  bv_bounds.set_height(toolbar_->y());
  if (bv_bounds.Contains(point))
    return HTNOWHERE;

  // If the point is somewhere else, delegate to the default implementation.
  return browser_view_->views::ClientView::NonClientHitTest(point);
}

//////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, views::LayoutManager implementation:

void BrowserViewLayout::Installed(views::View* host) {
  toolbar_ = NULL;
  contents_split_ = NULL;
  contents_container_ = NULL;
  infobar_container_ = NULL;
  download_shelf_ = NULL;
  active_bookmark_bar_ = NULL;
  tabstrip_ = NULL;
  browser_view_ = static_cast<BrowserView*>(host);
}

void BrowserViewLayout::Uninstalled(views::View* host) {}

void BrowserViewLayout::ViewAdded(views::View* host, views::View* view) {
  switch (view->GetID()) {
    case VIEW_ID_CONTENTS_SPLIT: {
      contents_split_ = static_cast<views::SingleSplitView*>(view);
      // We're installed as the LayoutManager before BrowserView creates the
      // contents, so we have to set contents_container_ here rather than in
      // Installed.
      contents_container_ = browser_view_->contents_;
      break;
    }
    case VIEW_ID_INFO_BAR_CONTAINER:
      infobar_container_ = view;
      break;
    case VIEW_ID_DOWNLOAD_SHELF:
      download_shelf_ = static_cast<DownloadShelfView*>(view);
      break;
    case VIEW_ID_BOOKMARK_BAR:
      active_bookmark_bar_ = static_cast<BookmarkBarView*>(view);
      break;
    case VIEW_ID_TOOLBAR:
      toolbar_ = static_cast<ToolbarView*>(view);
      break;
    case VIEW_ID_TAB_STRIP:
      tabstrip_ = static_cast<BaseTabStrip*>(view);
      break;
  }
}

void BrowserViewLayout::ViewRemoved(views::View* host, views::View* view) {
  switch (view->GetID()) {
    case VIEW_ID_BOOKMARK_BAR:
      active_bookmark_bar_ = NULL;
      break;
  }
}

void BrowserViewLayout::Layout(views::View* host) {
  vertical_layout_rect_ = browser_view_->GetLocalBounds(true);
  int top = LayoutTabStrip();
  if (browser_view_->IsTabStripVisible() && !browser_view_->UseVerticalTabs()) {
    tabstrip_->SetBackgroundOffset(gfx::Point(
        tabstrip_->MirroredX() + browser_view_->MirroredX(),
        browser_view_->frame()->GetHorizontalTabStripVerticalOffset(false)));
  }
  top = LayoutToolbar(top);
  top = LayoutBookmarkAndInfoBars(top);
  int bottom = LayoutDownloadShelf(browser_view_->height());
  int active_top_margin = GetTopMarginForActiveContent();
  top -= active_top_margin;
  contents_container_->SetActiveTopMargin(active_top_margin);
  LayoutTabContents(top, bottom);
  // This must be done _after_ we lay out the TabContents since this
  // code calls back into us to find the bounding box the find bar
  // must be laid out within, and that code depends on the
  // TabContentsContainer's bounds being up to date.
  if (browser()->HasFindBarController()) {
    browser()->GetFindBarController()->find_bar()->MoveWindowIfNecessary(
        gfx::Rect(), true);
  }
}

// Return the preferred size which is the size required to give each
// children their respective preferred size.
gfx::Size BrowserViewLayout::GetPreferredSize(views::View* host) {
  return gfx::Size();
}

//////////////////////////////////////////////////////////////////////////////
// BrowserViewLayout, private:

Browser* BrowserViewLayout::browser() {
  return browser_view_->browser();
}

const Browser* BrowserViewLayout::browser() const {
  return browser_view_->browser();
}

int BrowserViewLayout::LayoutTabStrip() {
  if (!browser_view_->IsTabStripVisible()) {
    tabstrip_->SetVisible(false);
    tabstrip_->SetBounds(0, 0, 0, 0);
    return 0;
  }

  gfx::Rect tabstrip_bounds(
      browser_view_->frame()->GetBoundsForTabStrip(tabstrip_));
  gfx::Point tabstrip_origin(tabstrip_bounds.origin());
  views::View::ConvertPointToView(browser_view_->GetParent(), browser_view_,
                                  &tabstrip_origin);
  tabstrip_bounds.set_origin(tabstrip_origin);

  if (browser_view_->UseVerticalTabs())
    vertical_layout_rect_.Inset(tabstrip_bounds.width(), 0, 0, 0);

  tabstrip_->SetVisible(true);
  tabstrip_->SetBounds(tabstrip_bounds);
  return browser_view_->UseVerticalTabs() ?
      tabstrip_bounds.y() : tabstrip_bounds.bottom();
}

int BrowserViewLayout::LayoutToolbar(int top) {
  int browser_view_width = vertical_layout_rect_.width();
  bool visible = browser_view_->IsToolbarVisible();
  toolbar_->location_bar()->SetFocusable(visible);
  int y = top;
  if (!browser_view_->UseVerticalTabs()) {
    y -= ((visible && browser_view_->IsTabStripVisible()) ?
          kToolbarTabStripVerticalOverlap : 0);
  }
  int height = visible ? toolbar_->GetPreferredSize().height() : 0;
  toolbar_->SetVisible(visible);
  toolbar_->SetBounds(vertical_layout_rect_.x(), y, browser_view_width, height);
  return y + height;
}

int BrowserViewLayout::LayoutBookmarkAndInfoBars(int top) {
  find_bar_y_ = top + browser_view_->y() - 1;
  if (active_bookmark_bar_) {
    // If we're showing the Bookmark bar in detached style, then we
    // need to show any Info bar _above_ the Bookmark bar, since the
    // Bookmark bar is styled to look like it's part of the page.
    if (active_bookmark_bar_->IsDetached())
      return LayoutBookmarkBar(LayoutInfoBar(top));
    // Otherwise, Bookmark bar first, Info bar second.
    top = std::max(toolbar_->bounds().bottom(), LayoutBookmarkBar(top));
  }
  find_bar_y_ = top + browser_view_->y() - 1;
  return LayoutInfoBar(top);
}

int BrowserViewLayout::LayoutBookmarkBar(int top) {
  DCHECK(active_bookmark_bar_);
  int y = top;
  if (!browser_view_->IsBookmarkBarVisible()) {
    active_bookmark_bar_->SetVisible(false);
    active_bookmark_bar_->SetBounds(0, y, browser_view_->width(), 0);
    return y;
  }

  active_bookmark_bar_->set_infobar_visible(InfobarVisible());
  int bookmark_bar_height = active_bookmark_bar_->GetPreferredSize().height();
  y -= kSeparationLineHeight + active_bookmark_bar_->GetToolbarOverlap(false);
  active_bookmark_bar_->SetVisible(true);
  active_bookmark_bar_->SetBounds(vertical_layout_rect_.x(), y,
                                  vertical_layout_rect_.width(),
                                  bookmark_bar_height);
  return y + bookmark_bar_height;
}

int BrowserViewLayout::LayoutInfoBar(int top) {
  bool visible = InfobarVisible();
  int height = visible ? infobar_container_->GetPreferredSize().height() : 0;
  infobar_container_->SetVisible(visible);
  infobar_container_->SetBounds(vertical_layout_rect_.x(), top,
                                vertical_layout_rect_.width(), height);
  return top + height;
}

// |browser_reserved_rect| is in browser_view_ coordinates.
// |future_source_bounds| is in |source|'s parent coordinates.
// |future_parent_offset| is required, since parent view is not moved yet.
// Note that |future_parent_offset| is relative to browser_view_, not to
// the parent view.
void BrowserViewLayout::UpdateReservedContentsRect(
    const gfx::Rect& browser_reserved_rect,
    TabContentsContainer* source,
    const gfx::Rect& future_source_bounds,
    const gfx::Point& future_parent_offset) {
  gfx::Point resize_corner_origin(browser_reserved_rect.origin());
  // Convert |resize_corner_origin| from browser_view_ to source's parent
  // coordinates.
  views::View::ConvertPointToView(browser_view_, source->GetParent(),
                                  &resize_corner_origin);
  // Create |reserved_rect| in source's parent coordinates.
  gfx::Rect reserved_rect(resize_corner_origin, browser_reserved_rect.size());
  // Apply source's parent future offset to it.
  reserved_rect.Offset(-future_parent_offset.x(), -future_parent_offset.y());
  if (future_source_bounds.Intersects(reserved_rect)) {
    // |source| is not properly positioned yet to use ConvertPointToView,
    // so convert it into |source|'s coordinates manually.
    reserved_rect.Offset(-future_source_bounds.x(), -future_source_bounds.y());
  } else {
    reserved_rect = gfx::Rect();
  }

  source->SetReservedContentsRect(reserved_rect);
}

void BrowserViewLayout::LayoutTabContents(int top, int bottom) {
  // The ultimate idea is to calcualte bounds and reserved areas for all
  // contents views first and then resize them all, so every view
  // (and its contents) is resized and laid out only once.

  // The views hierarcy (see browser_view.h for more details):
  // 1) Sidebar is not allowed:
  //     contents_split_ -> [contents_container_ | devtools]
  // 2) Sidebar is allowed:
  //     contents_split_ ->
  //         [sidebar_split -> [contents_container_ | sidebar]] | devtools

  gfx::Rect sidebar_split_bounds;
  gfx::Rect contents_bounds;
  gfx::Rect sidebar_bounds;
  gfx::Rect devtools_bounds;

  gfx::Rect contents_split_bounds(vertical_layout_rect_.x(), top,
                                  vertical_layout_rect_.width(),
                                  std::max(0, bottom - top));
  contents_split_->CalculateChildrenBounds(
      contents_split_bounds, &sidebar_split_bounds, &devtools_bounds);
  gfx::Point contents_split_offset(
      contents_split_bounds.x() - contents_split_->bounds().x(),
      contents_split_bounds.y() - contents_split_->bounds().y());
  gfx::Point sidebar_split_offset(contents_split_offset);
  sidebar_split_offset.Offset(sidebar_split_bounds.x(),
                              sidebar_split_bounds.y());

  views::SingleSplitView* sidebar_split = browser_view_->sidebar_split_;
  if (sidebar_split) {
    DCHECK(sidebar_split == contents_split_->GetChildViewAt(0));
    sidebar_split->CalculateChildrenBounds(
        sidebar_split_bounds, &contents_bounds, &sidebar_bounds);
  } else {
    contents_bounds = sidebar_split_bounds;
  }

  // Layout resize corner, sidebar mini tabs and calculate reserved contents
  // rects here as all contents view bounds are already determined, but not yet
  // set at this point, so contents will be laid out once at most.
  // TODO(alekseys): layout sidebar minitabs and adjust reserved rect
  // accordingly.
  gfx::Rect browser_reserved_rect;
  if (!browser_view_->frame_->GetWindow()->IsMaximized() &&
      !browser_view_->frame_->GetWindow()->IsFullscreen()) {
    gfx::Size resize_corner_size = browser_view_->GetResizeCornerSize();
    if (!resize_corner_size.IsEmpty()) {
      gfx::Rect bounds = browser_view_->GetLocalBounds(false);
      gfx::Point resize_corner_origin(
          bounds.right() - resize_corner_size.width(),
          bounds.bottom() - resize_corner_size.height());
      browser_reserved_rect =
          gfx::Rect(resize_corner_origin, resize_corner_size);
    }
  }

  UpdateReservedContentsRect(browser_reserved_rect,
                             browser_view_->contents_container_,
                             contents_bounds,
                             sidebar_split_offset);
  if (sidebar_split) {
    UpdateReservedContentsRect(browser_reserved_rect,
                               browser_view_->sidebar_container_,
                               sidebar_bounds,
                               sidebar_split_offset);
  }
  UpdateReservedContentsRect(browser_reserved_rect,
                             browser_view_->devtools_container_,
                             devtools_bounds,
                             contents_split_offset);

  // Now it's safe to actually resize all contents views in the hierarchy.
  contents_split_->SetBounds(contents_split_bounds);
  if (sidebar_split)
    sidebar_split->SetBounds(sidebar_split_bounds);
}

int BrowserViewLayout::GetTopMarginForActiveContent() {
  if (!active_bookmark_bar_ || !browser_view_->IsBookmarkBarVisible() ||
      !active_bookmark_bar_->IsDetached()) {
    return 0;
  }

  if (contents_split_->GetChildViewAt(1) &&
      contents_split_->GetChildViewAt(1)->IsVisible())
    return 0;

  if (SidebarManager::IsSidebarAllowed()) {
    views::View* sidebar_split = contents_split_->GetChildViewAt(0);
    if (sidebar_split->GetChildViewAt(1) &&
        sidebar_split->GetChildViewAt(1)->IsVisible())
      return 0;
  }

  // Adjust for separator.
  return active_bookmark_bar_->height() - kSeparationLineHeight;
}

int BrowserViewLayout::LayoutDownloadShelf(int bottom) {
  // Re-layout the shelf either if it is visible or if it's close animation
  // is currently running.
  if (browser_view_->IsDownloadShelfVisible() ||
      (download_shelf_ && download_shelf_->IsClosing())) {
    bool visible = browser()->SupportsWindowFeature(
        Browser::FEATURE_DOWNLOADSHELF);
    DCHECK(download_shelf_);
    int height = visible ? download_shelf_->GetPreferredSize().height() : 0;
    download_shelf_->SetVisible(visible);
    download_shelf_->SetBounds(vertical_layout_rect_.x(), bottom - height,
                               vertical_layout_rect_.width(), height);
    download_shelf_->Layout();
    bottom -= height;
  }
  return bottom;
}

bool BrowserViewLayout::InfobarVisible() const {
  // NOTE: Can't check if the size IsEmpty() since it's always 0-width.
  return browser()->SupportsWindowFeature(Browser::FEATURE_INFOBAR) &&
      (infobar_container_->GetPreferredSize().height() != 0);
}
