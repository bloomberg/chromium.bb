// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_shelf.h"

#include <algorithm>

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/extensions/extension_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "skia/ext/skia_utils.h"
#include "views/controls/label.h"
#include "views/screen.h"
#include "views/widget/root_view.h"

namespace {

// Margins around the content.
static const int kTopMargin = 2;
static const int kBottomMargin = 2;
static const int kLeftMargin = 0;
static const int kRightMargin = 0;

// Padding on left and right side of an extension toolstrip.
static const int kToolstripPadding = 2;

// Width of the toolstrip divider.
static const int kToolstripDividerWidth = 2;

// Preferred height of the ExtensionShelf.
static const int kShelfHeight = 29;

// Height of the toolstrip within the shelf.
static const int kToolstripHeight = kShelfHeight - (kTopMargin + kBottomMargin);

// Colors for the ExtensionShelf.
static const SkColor kBackgroundColor = SkColorSetRGB(230, 237, 244);
static const SkColor kBorderColor = SkColorSetRGB(201, 212, 225);
static const SkColor kDividerHighlightColor = SkColorSetRGB(247, 250, 253);

// Text colors for the handle
static const SkColor kHandleTextColor = SkColorSetRGB(6, 45, 117);
static const SkColor kHandleTextHighlightColor =
    SkColorSetARGB(200, 255, 255, 255);

// Handle padding
static const int kHandlePadding = 4;

// TODO(erikkay) convert back to a gradient when Glen figures out the
// specs.
// static const SkColor kBackgroundColor = SkColorSetRGB(237, 244, 252);
// static const SkColor kTopGradientColor = SkColorSetRGB(222, 234, 248);

// Delays for showing and hiding the shelf handle.
static const int kHideDelayMs = 500;

}  // namespace


// A view that holds the place for a toolstrip in the shelf while the toolstrip
// is being dragged or moved.
// TODO(erikkay) this should draw a dimmed out version of the toolstrip.
class ExtensionShelf::PlaceholderView : public views::View {
 public:
  PlaceholderView() {}

  void SetWidth(int width) {
    SetBounds(x(), y(), width, height());
    PreferredSizeChanged();
  }

  // ExtensionShelf resizes its views to their preferred size at layout,
  // so just always prefer the current size.
  gfx::Size GetPreferredSize() { return size(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(PlaceholderView);
};

// A wrapper class for the ExtensionHost displayed as a toolstrip.
// The class itself also acts as the View for the handle of the toolstrip
// it represents.
class ExtensionShelf::Toolstrip : public views::View,
                                  public BrowserBubble::Delegate {
 public:
  Toolstrip(ExtensionShelf* shelf, ExtensionHost* host,
            const Extension::ToolstripInfo& info);
  virtual ~Toolstrip();

  // View
  virtual void Paint(gfx::Canvas* canvas);
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void OnMouseEntered(const views::MouseEvent& event);
  virtual void OnMouseExited(const views::MouseEvent& event);
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual bool IsFocusable() const { return true; }
  virtual void ChildPreferredSizeChanged(View* child);

  // Retrieves the handle for this toolstrip, creating it if necessary.
  BrowserBubble* GetHandle();

  // Adjust the size and position of the handle.
  void LayoutHandle();

  // Is the handle for this toolstrip currently visible.
  bool handle_visible() { return (handle_.get() && handle_->visible()); }

  // Is the toolstrip opened in expanded mode.
  bool expanded() { return expanded_; }

  // Is the toolstrip being dragged.
  bool dragging() { return dragging_; }

  // Accessors for the host and its view.
  ExtensionHost* host() const { return host_; }
  ExtensionView* view() const { return host_->view(); }

  // The view that's currently displayed in the shelf.  This could be
  // either the ExtensionView or |placeholder_view_| depending on the
  // current state.
  View* GetShelfView() const {
    if (placeholder_view_)
      return placeholder_view_;
    return view();
  }

  // Detaching the toolstrip from the shelf means that the ExtensionView is
  // displayed inside of the BrowserBubble rather than the shelf. This may
  // still visually appear to be part of the shelf (expanded) or completely
  // separate (dragging).
  // If |browser| is true, it also will detach/attach to the browser window.
  void DetachFromShelf(bool browser);
  void AttachToShelf(bool browser);

  // Show / Hide the shelf handle.
  void ShowShelfHandle();
  void DoShowShelfHandle();
  void HideShelfHandle(int delay_ms);
  void DoHideShelfHandle();
  void StopHandleTimer();

  // Expand / Collapse
  void Expand(int height, const GURL& url);
  void Collapse(const GURL& url);

  // BrowserBubble::Delegate
  virtual void BubbleBrowserWindowMoved(BrowserBubble* bubble);
  virtual void BubbleBrowserWindowClosing(BrowserBubble* bubble);

 private:
  // The actual renderer that this toolstrip contains.
  ExtensionHost* host_;

  // Manifest definition of this toolstrip.
  Extension::ToolstripInfo info_;

  // The handle is a BrowserBubble so that it can exist as an independent,
  // floating window.  It also acts as the container for the ExtensionView when
  // it's being dragged.
  scoped_ptr<BrowserBubble> handle_;

  // Used for drawing the name of the extension in the handle.
  scoped_ptr<views::Label> title_;

  // Pointer back to the containing shelf.
  ExtensionShelf* shelf_;

  // When dragging, a placeholder view is put into the shelf to hold space
  // for the ExtensionView.  This view is parent owned when it's in the view
  // hierarchy, so there's no ownership issues here.
  PlaceholderView* placeholder_view_;

  // Current state of the toolstrip, currently can't both be expanded and
  // dragging.
  // TODO(erikkay) Support dragging while expanded.
  bool dragging_;
  bool expanded_;

  // If dragging, where did the drag start from.
  gfx::Point initial_drag_location_;

  // Timers for tracking mouse hovering.
  ScopedRunnableMethodFactory<ExtensionShelf::Toolstrip> timer_factory_;

  DISALLOW_COPY_AND_ASSIGN(Toolstrip);
};

ExtensionShelf::Toolstrip::Toolstrip(ExtensionShelf* shelf,
                                     ExtensionHost* host,
                                     const Extension::ToolstripInfo& info)
    : host_(host),
      info_(info),
      shelf_(shelf),
      placeholder_view_(NULL),
      dragging_(false),
      expanded_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(timer_factory_(this)) {
  DCHECK(host->view());
  // We're owned by shelf_, not the bubble that we get inserted in and out of.
  SetParentOwned(false);

  std::wstring name = UTF8ToWide(host_->extension()->name());
  // |title_| isn't actually put in the view hierarchy.  We just use it
  // to draw in place.  The reason for this is so that we can properly handle
  // the various mouse events necessary for hovering and dragging.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  title_.reset(new views::Label(name, rb.GetFont(ResourceBundle::BaseFont)));
  title_->SetColor(kHandleTextColor);
  title_->SetDrawHighlighted(true);
  title_->SetHighlightColor(kHandleTextHighlightColor);
  title_->SetBounds(kHandlePadding, kHandlePadding, 100, 100);
  title_->SizeToPreferredSize();

  SizeToPreferredSize();
}

ExtensionShelf::Toolstrip::~Toolstrip() {
  if (handle_.get() && handle_->attached())
    handle_->DetachFromBrowser();
}

void ExtensionShelf::Toolstrip::Paint(gfx::Canvas* canvas) {
  canvas->FillRectInt(kBackgroundColor, 0, 0, width(), height());
  canvas->FillRectInt(kBorderColor, 0, 0, width(), 1);
  canvas->FillRectInt(kBorderColor, 0, 0, 1, height() - 1);
  canvas->FillRectInt(kBorderColor, width() - 1, 0, 1, height() - 1);
  int ext_width = view()->width() + kToolstripPadding +
      kToolstripDividerWidth;
  if (ext_width < width()) {
    canvas->FillRectInt(kBorderColor, ext_width, height() - 1,
                        width() - ext_width, 1);
  }

  // Draw the title using a Label as a stamp.
  // See constructor for comment about this.
  title_->ProcessPaint(canvas);

  if (dragging_) {
    // when we're dragging, draw the bottom border.
    canvas->FillRectInt(kBorderColor, 0, height() - 1, width(), 1);
  }
}

gfx::Size ExtensionShelf::Toolstrip::GetPreferredSize() {
  gfx::Size sz = title_->GetPreferredSize();
  sz.set_width(std::max(view()->width(), sz.width()));
  if (!expanded_)
    sz.Enlarge(2 + kHandlePadding * 2, kHandlePadding * 2);
  if (dragging_ || expanded_) {
    gfx::Size extension_size = view()->GetPreferredSize();
    sz.Enlarge(0, extension_size.height() + 2);
  }
  return sz;
}

void ExtensionShelf::Toolstrip::Layout() {
  if (dragging_ || expanded_) {
    int y = title_->bounds().bottom() + kHandlePadding + 1;
    view()->SetBounds(1, y, view()->width(), view()->height());
  }
}

void ExtensionShelf::Toolstrip::OnMouseEntered(const views::MouseEvent& event) {
  if (dragging_ || expanded_)
    return;
  ShowShelfHandle();
}

void ExtensionShelf::Toolstrip::OnMouseExited(const views::MouseEvent& event) {
  if (dragging_ || expanded_)
    return;
  HideShelfHandle(kHideDelayMs);
}

bool ExtensionShelf::Toolstrip::OnMousePressed(const views::MouseEvent& event) {
  initial_drag_location_ = event.location();
  return true;
}

bool ExtensionShelf::Toolstrip::OnMouseDragged(const views::MouseEvent& event) {
  if (expanded_) {
    // Do nothing for now.
  } else if (!dragging_) {
    int y_delta = abs(initial_drag_location_.y() - event.location().y());
    int x_delta = abs(initial_drag_location_.x() - event.location().x());
    if (y_delta > GetVerticalDragThreshold() ||
        x_delta > GetHorizontalDragThreshold()) {
      dragging_ = true;
      StopHandleTimer();
      DetachFromShelf(true);
    }
  } else {
    // When freely dragging a window, you can really only trust the
    // actual screen point.  Local coordinate conversions don't work.
    gfx::Point screen = views::Screen::GetCursorScreenPoint();

    // However, the handle is actually a child of the browser window
    // so we need to convert it back to local coordinates.
    gfx::Point origin(0, 0);
    views::View::ConvertPointToScreen(shelf_->GetRootView(), &origin);
    screen.set_x(screen.x() - origin.x() - initial_drag_location_.x());
    screen.set_y(screen.y() - origin.y() - initial_drag_location_.y());

    // TODO(erikkay) as this gets dragged around, update the placeholder view
    // on the shelf to show where it will get dropped to.
    handle_->MoveTo(screen.x(), screen.y());
  }
  return true;
}

void ExtensionShelf::Toolstrip::OnMouseReleased(const views::MouseEvent& event,
                                                bool canceled) {
  StopHandleTimer();
  if (dragging_) {
    // Drop the toolstrip roughly where it is now.
    views::View::OnMouseReleased(event, canceled);
    dragging_ = false;
    // |this| and |shelf_| are in different view hierarchies, so we need to
    // convert to screen coordinates and back again to map locations.
    gfx::Point loc = event.location();
    View::ConvertPointToScreen(this, &loc);
    View::ConvertPointToView(NULL, shelf_, &loc);
    shelf_->DropExtension(this, loc, canceled);
    AttachToShelf(true);
  } else if (!canceled) {
    // Toggle mole to either expanded or collapsed.
    // TODO(erikkay) If there's no valid URL in the manifest, should we
    // post an event to the toolstrip in this case?
    if (expanded_) {
      if (info_.toolstrip.is_valid())
        shelf_->CollapseToolstrip(host_, info_.toolstrip);
    } else {
     if (info_.mole.is_valid())
       shelf_->ExpandToolstrip(host_, info_.mole, info_.mole_height);
    }
  }
}

BrowserBubble* ExtensionShelf::Toolstrip::GetHandle() {
  if (!handle_.get()) {
    handle_.reset(new BrowserBubble(this, shelf_->GetWidget(),
                                    gfx::Point(0, 0)));
    handle_->set_delegate(this);
    LayoutHandle();
  }
  return handle_.get();
}

void ExtensionShelf::Toolstrip::LayoutHandle() {
  if (!handle_.get())
    return;

  int handle_width = std::max(view()->width(), width());
  gfx::Point origin(-kToolstripPadding, -(height() + kToolstripPadding - 1));
  if (expanded_) {
    origin.set_y(GetShelfView()->height() - height());
    views::View::ConvertPointToView(GetShelfView(), shelf_->GetRootView(),
                                    &origin);
  } else {
    views::View::ConvertPointToWidget(view(), &origin);
  }
  SetBounds(0, 0, handle_width, height());
  handle_->SetBounds(origin.x(), origin.y(), handle_width, height());
  handle_->ResizeToView();
}

void ExtensionShelf::Toolstrip::ChildPreferredSizeChanged(View* child) {
  if (child == view()) {
    child->SizeToPreferredSize();
    Layout();
    if (expanded_) {
      LayoutHandle();
      placeholder_view_->SetWidth(child->width());
      shelf_->Layout();
    }
  }
}

void ExtensionShelf::Toolstrip::BubbleBrowserWindowMoved(
    BrowserBubble* bubble) {
  HideShelfHandle(0);
}

void ExtensionShelf::Toolstrip::BubbleBrowserWindowClosing(
    BrowserBubble* bubble) {
  DoHideShelfHandle();
}

void ExtensionShelf::Toolstrip::DetachFromShelf(bool browserDetach) {
  DCHECK(handle_.get());
  DCHECK(!placeholder_view_);
  if (browserDetach && handle_->attached())
    handle_->DetachFromBrowser();

  // Construct a placeholder view to replace the view.
  placeholder_view_ = new PlaceholderView();
  placeholder_view_->SetBounds(view()->bounds());
  shelf_->AddChildView(placeholder_view_);

  AddChildView(view());
  SizeToPreferredSize();
  handle_->ResizeToView();
  Layout();
}

void ExtensionShelf::Toolstrip::AttachToShelf(bool browserAttach) {
  DCHECK(handle_.get());
  DCHECK(placeholder_view_);
  if (browserAttach && !handle_->attached())
    handle_->AttachToBrowser();

  // Move the view back into the shelf and remove the old placeholder.
  shelf_->AddChildView(view());

  // The size of the view may have changed, so just set the position.
  view()->SetX(placeholder_view_->x());
  view()->SetY(placeholder_view_->y());

  // Remove the old placeholder.
  shelf_->RemoveChildView(placeholder_view_);
  delete placeholder_view_;
  placeholder_view_ = NULL;

  SizeToPreferredSize();
  Layout();
  shelf_->Layout();
}

void ExtensionShelf::Toolstrip::DoShowShelfHandle() {
  GetHandle();
  if (!handle_->visible()) {
    LayoutHandle();
    handle_->Show();
  }
}

void ExtensionShelf::Toolstrip::DoHideShelfHandle() {
  if (!handle_visible())
    return;
  handle_->Hide();
  if (handle_->attached())
    handle_->DetachFromBrowser();
  handle_.reset(NULL);
  shelf_->Layout();
}

void ExtensionShelf::Toolstrip::StopHandleTimer() {
  if (!timer_factory_.empty())
    timer_factory_.RevokeAll();
}

void ExtensionShelf::Toolstrip::Expand(int height, const GURL& url) {
  DCHECK(!expanded_);

  DoShowShelfHandle();

  expanded_ = true;
  view()->set_is_toolstrip(!expanded_);

  bool navigate = (!url.is_empty() && url != host_->GetURL());
  if (navigate)
    host_->NavigateToURL(url);

  StopHandleTimer();
  DetachFromShelf(false);

  gfx::Size extension_size = view()->GetPreferredSize();
  extension_size.set_height(height);
  view()->SetPreferredSize(extension_size);
  LayoutHandle();

  // This is to prevent flickering as the page loads and lays out.
  // Once the navigation is finished, ExtensionView will wind up setting
  // visibility to true.
  if (navigate)
    view()->SetVisible(false);
}

void ExtensionShelf::Toolstrip::Collapse(const GURL& url) {
  DCHECK(expanded_);
  expanded_ = false;
  view()->set_is_toolstrip(!expanded_);

  gfx::Size extension_size = view()->GetPreferredSize();
  extension_size.set_height(kToolstripHeight);
  view()->SetPreferredSize(extension_size);
  AttachToShelf(false);

  if (!url.is_empty() && url != host_->GetURL()) {
    host_->NavigateToURL(url);

    // This is to prevent flickering as the page loads and lays out.
    // Once the navigation is finished, ExtensionView will wind up setting
    // visibility to true.
    view()->SetVisible(false);
  }

  // Must use the delay due to bug 18248.
  HideShelfHandle(kHideDelayMs);
}

void ExtensionShelf::Toolstrip::ShowShelfHandle() {
  StopHandleTimer();
  if (handle_visible())
    return;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      timer_factory_.NewRunnableMethod(
          &ExtensionShelf::Toolstrip::DoShowShelfHandle),
      1000);
}

void ExtensionShelf::Toolstrip::HideShelfHandle(int delay_ms) {
  StopHandleTimer();
  if (!handle_visible() || dragging_ || expanded_)
    return;
  if (delay_ms) {
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        timer_factory_.NewRunnableMethod(
            &ExtensionShelf::Toolstrip::DoHideShelfHandle),
        delay_ms);
  } else {
    DoHideShelfHandle();
  }
}

////////////////////////////////////////////////////////////////////////////////

ExtensionShelf::ExtensionShelf(Browser* browser)
    : model_(browser->extension_shelf_model()) {
  model_->AddObserver(this);
  LoadFromModel();
  EnableCanvasFlippingForRTLUI(true);
}

ExtensionShelf::~ExtensionShelf() {
  if (model_) {
    int count = model_->count();
    for (int i = 0; i < count; ++i) {
      delete ToolstripAtIndex(i);
      model_->SetToolstripDataAt(i, NULL);
    }
    model_->RemoveObserver(this);
  }
}

void ExtensionShelf::Paint(gfx::Canvas* canvas) {
#if 0
  // TODO(erikkay) re-enable this when Glen has the gradient values worked out.
  SkPaint paint;
  paint.setShader(skia::CreateGradientShader(0,
                                             height(),
                                             kTopGradientColor,
                                             kBackgroundColor))->safeUnref();
  canvas->FillRectInt(0, 0, width(), height(), paint);
#else
  canvas->FillRectInt(kBackgroundColor, 0, 0, width(), height());
#endif

  canvas->FillRectInt(kBorderColor, 0, 0, width(), 1);
  canvas->FillRectInt(kBorderColor, 0, height() - 1, width(), 1);

  int count = GetChildViewCount();
  for (int i = 0; i < count; ++i) {
    int right = GetChildViewAt(i)->bounds().right() + kToolstripPadding;
    int h = height() - 2;
    canvas->FillRectInt(kBorderColor, right, 1, 1, h);
    canvas->FillRectInt(kDividerHighlightColor, right + 1, 1, 1, h);
  }

  SkRect background_rect = {
      SkIntToScalar(0),
      SkIntToScalar(1),
      SkIntToScalar(1),
      SkIntToScalar(height() - 2)};
  InitBackground(canvas, background_rect);
}

gfx::Size ExtensionShelf::GetPreferredSize() {
  if (model_->count())
    return gfx::Size(0, kShelfHeight);
  return gfx::Size(0, 0);
}

void ExtensionShelf::ChildPreferredSizeChanged(View* child) {
  Toolstrip *toolstrip = ToolstripForView(static_cast<ExtensionView*>(child));
  if (!toolstrip)
    return;
  Layout();
}

void ExtensionShelf::Layout() {
  if (!GetParent())
    return;
  if (!model_)
    return;

  int x = kLeftMargin;
  int y = kTopMargin;
  int content_height = height() - kTopMargin - kBottomMargin;
  int max_x = width() - kRightMargin;

  int count = model_->count();
  for (int i = 0; i < count; ++i) {
    x += kToolstripPadding;  // left padding
    Toolstrip* toolstrip = ToolstripAtIndex(i);
    if (!toolstrip)  // can be NULL while in the process of removing
      continue;
    View* view = toolstrip->GetShelfView();
    gfx::Size pref = view->GetPreferredSize();
    int next_x = x + pref.width() + kToolstripPadding;  // right padding
    if (view == toolstrip->view())
      toolstrip->view()->set_is_clipped(next_x >= max_x);
    view->SetBounds(x, y, pref.width(), content_height);
    view->Layout();
    if (toolstrip->handle_visible())
      toolstrip->LayoutHandle();
    x = next_x + kToolstripDividerWidth;
  }
  SchedulePaint();
}

void ExtensionShelf::OnMouseEntered(const views::MouseEvent& event) {
}

void ExtensionShelf::OnMouseExited(const views::MouseEvent& event) {
}

bool ExtensionShelf::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_TOOLBAR;
  return true;
}

bool ExtensionShelf::GetAccessibleName(std::wstring* name) {
  DCHECK(name);

  if (!accessible_name_.empty()) {
    name->assign(accessible_name_);
    return true;
  }
  return false;
}

void ExtensionShelf::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

void ExtensionShelf::ToolstripInsertedAt(ExtensionHost* host,
                                         int index) {
  model_->SetToolstripDataAt(index,
      new Toolstrip(this, host, model_->ToolstripAt(index).info));

  bool had_views = GetChildViewCount() > 0;
  ExtensionView* view = host->view();
  if (!background_.empty())
    view->SetBackground(background_);
  AddChildView(view);
  view->SetContainer(this);
  if (!had_views)
    PreferredSizeChanged();
  Layout();
}

void ExtensionShelf::ToolstripRemovingAt(ExtensionHost* host, int index) {
  // Delete the Toolstrip view and remove it from the model.
  Toolstrip* toolstrip = ToolstripAtIndex(index);
  View* view = toolstrip->GetShelfView();
  RemoveChildView(view);
  delete toolstrip;
  model_->SetToolstripDataAt(index, NULL);

  // Technically, the toolstrip is still in the model at this point, but
  // the Layout code handles this case.
  Layout();
}

void ExtensionShelf::ToolstripDraggingFrom(ExtensionHost* host, int index) {
}

void ExtensionShelf::ToolstripMoved(ExtensionHost* host, int from_index,
                                    int to_index) {
  Layout();
}

void ExtensionShelf::ToolstripChanged(ExtensionShelfModel::iterator toolstrip) {
  Toolstrip* t = static_cast<Toolstrip*>(toolstrip->data);
  if (toolstrip->height > 0) {
    if (!t->expanded()) {
      t->Expand(toolstrip->height, toolstrip->url);
    }
  } else if (t->expanded()) {
    t->Collapse(toolstrip->url);
  }
}

void ExtensionShelf::ExtensionShelfEmpty() {
  PreferredSizeChanged();
}

void ExtensionShelf::ShelfModelReloaded() {
  // None of the child views are parent owned, so nothing is being leaked here.
  RemoveAllChildViews(false);
  LoadFromModel();
}

void ExtensionShelf::ShelfModelDeleting() {
  int count = model_->count();
  for (int i = 0; i < count; ++i) {
    delete ToolstripAtIndex(i);
    model_->SetToolstripDataAt(i, NULL);
  }
  model_->RemoveObserver(this);
  model_ = NULL;
}

void ExtensionShelf::OnExtensionMouseEvent(ExtensionView* view) {
  Toolstrip *toolstrip = ToolstripForView(view);
  if (toolstrip)
    toolstrip->ShowShelfHandle();
}

void ExtensionShelf::OnExtensionMouseLeave(ExtensionView* view) {
  Toolstrip *toolstrip = ToolstripForView(view);
  if (toolstrip)
    toolstrip->HideShelfHandle(kHideDelayMs);
}

void ExtensionShelf::DropExtension(Toolstrip* toolstrip, const gfx::Point& pt,
                                   bool cancel) {
  Toolstrip* dest_toolstrip = ToolstripAtX(pt.x());
  if (!dest_toolstrip) {
    if (pt.x() > 0)
      dest_toolstrip = ToolstripAtIndex(model_->count() - 1);
    else
      dest_toolstrip = ToolstripAtIndex(0);
  }
  if (toolstrip == dest_toolstrip)
    return;
  int from = model_->IndexOfHost(toolstrip->host());
  int to = model_->IndexOfHost(dest_toolstrip->host());
  DCHECK(from != to);
  model_->MoveToolstripAt(from, to);
}

void ExtensionShelf::ExpandToolstrip(ExtensionHost* host, const GURL& url,
                                     int height) {
  ExtensionShelfModel::iterator toolstrip = model_->ToolstripForHost(host);
  model_->ExpandToolstrip(toolstrip, url, height);
}

void ExtensionShelf::CollapseToolstrip(ExtensionHost* host, const GURL& url) {
  ExtensionShelfModel::iterator toolstrip = model_->ToolstripForHost(host);
  model_->CollapseToolstrip(toolstrip, url);
}

void ExtensionShelf::InitBackground(gfx::Canvas* canvas, const SkRect& subset) {
  if (!background_.empty())
    return;

  const SkBitmap& background = canvas->getDevice()->accessBitmap(false);

  // Extract the correct subset of the toolstrip background into a bitmap. We
  // must use a temporary here because extractSubset() returns a bitmap that
  // references pixels in the original one and we want to actually make a copy
  // that will have a long lifetime.
  SkBitmap temp;
  temp.setConfig(background.config(),
                 static_cast<int>(subset.width()),
                 static_cast<int>(subset.height()));

  SkRect mapped_subset = subset;
  bool result = canvas->getTotalMatrix().mapRect(&mapped_subset);
  DCHECK(result);

  SkIRect isubset;
  mapped_subset.round(&isubset);
  result = background.extractSubset(&temp, isubset);
  if (!result)
    return;

  temp.copyTo(&background_, temp.config());
  DCHECK(background_.readyToDraw());

  // Tell all extension views about the new background
  int count = model_->count();
  for (int i = 0; i < count; ++i)
    ToolstripAtIndex(i)->view()->SetBackground(background_);
}

ExtensionShelf::Toolstrip* ExtensionShelf::ToolstripAtX(int x) {
  int count = model_->count();
  if (count == 0)
    return NULL;

  if (x < 0)
    return NULL;

  for (int i = 0; i < count; ++i) {
    Toolstrip* toolstrip = ToolstripAtIndex(i);
    View* view = toolstrip->GetShelfView();
    if (x > (view->x() + view->width() + kToolstripPadding))
      continue;
    return toolstrip;
  }

  return NULL;
}

ExtensionShelf::Toolstrip* ExtensionShelf::ToolstripAtIndex(int index) {
  return static_cast<Toolstrip*>(model_->ToolstripAt(index).data);
}

ExtensionShelf::Toolstrip* ExtensionShelf::ToolstripForView(
    ExtensionView* view) {
  int count = model_->count();
  for (int i = 0; i < count; ++i) {
    Toolstrip* toolstrip = ToolstripAtIndex(i);
    if (view == toolstrip->view())
      return toolstrip;
  }
  return NULL;
}

void ExtensionShelf::LoadFromModel() {
  int count = model_->count();
  for (int i = 0; i < count; ++i)
    ToolstripInsertedAt(model_->ToolstripAt(i).host, i);
}
