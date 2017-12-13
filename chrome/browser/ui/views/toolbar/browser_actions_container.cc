// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/command.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "extensions/common/feature_switch.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace {

// Returns the ToolbarView for the given |browser|.
ToolbarView* GetToolbarView(Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)->toolbar();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// BrowserActionsContainer::DropPosition

struct BrowserActionsContainer::DropPosition {
  DropPosition(size_t row, size_t icon_in_row);

  // The (0-indexed) row into which the action will be dropped.
  size_t row;

  // The (0-indexed) icon in the row before the action will be dropped.
  size_t icon_in_row;
};

BrowserActionsContainer::DropPosition::DropPosition(
    size_t row, size_t icon_in_row)
    : row(row), icon_in_row(icon_in_row) {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserActionsContainer

BrowserActionsContainer::BrowserActionsContainer(
    Browser* browser,
    BrowserActionsContainer* main_container)
    : toolbar_actions_bar_(new ToolbarActionsBar(
          this,
          browser,
          main_container ? main_container->toolbar_actions_bar_.get()
                         : nullptr)),
      browser_(browser),
      main_container_(main_container) {
  set_id(VIEW_ID_BROWSER_ACTION_TOOLBAR);

  if (!ShownInsideMenu()) {
    resize_animation_.reset(new gfx::SlideAnimation(this));
    resize_area_ = new views::ResizeArea(this);
    AddChildView(resize_area_);

    const int kWarningImages[] = IMAGE_GRID(IDR_DEVELOPER_MODE_HIGHLIGHT);
    warning_highlight_painter_ =
        views::Painter::CreateImageGridPainter(kWarningImages);
  }
}

BrowserActionsContainer::~BrowserActionsContainer() {
  if (active_bubble_)
    active_bubble_->GetWidget()->Close();
  // We should synchronously receive the OnWidgetClosing() event, so we should
  // always have cleared the active bubble by now.
  DCHECK(!active_bubble_);

  toolbar_actions_bar_->DeleteActions();
  // All views should be removed as part of ToolbarActionsBar::DeleteActions().
  DCHECK(toolbar_action_views_.empty());
}

std::string BrowserActionsContainer::GetIdAt(size_t index) const {
  return toolbar_action_views_[index]->view_controller()->GetId();
}

ToolbarActionView* BrowserActionsContainer::GetViewForId(
    const std::string& id) {
  for (const auto& view : toolbar_action_views_) {
    if (view->view_controller()->GetId() == id)
      return view.get();
  }
  return nullptr;
}

void BrowserActionsContainer::RefreshToolbarActionViews() {
  toolbar_actions_bar_->Update();
}

size_t BrowserActionsContainer::VisibleBrowserActions() const {
  size_t visible_actions = 0;
  for (const auto& view : toolbar_action_views_) {
    if (view->visible())
      ++visible_actions;
  }
  return visible_actions;
}

size_t BrowserActionsContainer::VisibleBrowserActionsAfterAnimation() const {
  if (!animating())
    return VisibleBrowserActions();

  return toolbar_actions_bar_->WidthToIconCount(animation_target_size_);
}

bool BrowserActionsContainer::ShownInsideMenu() const {
  return main_container_ != nullptr;
}

void BrowserActionsContainer::OnToolbarActionViewDragDone() {
  toolbar_actions_bar_->OnDragEnded();
}

views::MenuButton* BrowserActionsContainer::GetOverflowReferenceView() {
  return static_cast<views::MenuButton*>(
      GetToolbarView(browser_)->app_menu_button());
}

void BrowserActionsContainer::AddViewForAction(
   ToolbarActionViewController* view_controller,
   size_t index) {
  ToolbarActionView* view = new ToolbarActionView(view_controller, this);
  toolbar_action_views_.insert(toolbar_action_views_.begin() + index,
                               base::WrapUnique(view));
  AddChildViewAt(view, index);
}

void BrowserActionsContainer::RemoveViewForAction(
    ToolbarActionViewController* action) {
  std::unique_ptr<ToolbarActionView> view;
  for (ToolbarActionViews::iterator iter = toolbar_action_views_.begin();
       iter != toolbar_action_views_.end(); ++iter) {
    if ((*iter)->view_controller() == action) {
      std::swap(view, *iter);
      toolbar_action_views_.erase(iter);
      break;
    }
  }
}

void BrowserActionsContainer::RemoveAllViews() {
  toolbar_action_views_.clear();
}

void BrowserActionsContainer::Redraw(bool order_changed) {
  if (!added_to_view_) {
    // We don't want to redraw before the view has been fully added to the
    // hierarchy.
    return;
  }

  // Don't allow resizing if the bar is highlighting.
  if (resize_area_)
    resize_area_->SetEnabled(!toolbar_actions_bar()->is_highlighting());

  std::vector<ToolbarActionViewController*> actions =
      toolbar_actions_bar_->GetActions();
  if (order_changed) {
    // Run through the views and compare them to the desired order. If something
    // is out of place, find the correct spot for it.
    for (int i = 0; i < static_cast<int>(actions.size()) - 1; ++i) {
      if (actions[i] != toolbar_action_views_[i]->view_controller()) {
        // Find where the correct view is (it's guaranteed to be after our
        // current index, since everything up to this point is correct).
        int j = i + 1;
        while (actions[i] != toolbar_action_views_[j]->view_controller())
          ++j;
        std::swap(toolbar_action_views_[i], toolbar_action_views_[j]);
        // Also move the view in the child views vector.
        ReorderChildView(toolbar_action_views_[i].get(), i);
      }
    }
  }

  Layout();
}

void BrowserActionsContainer::ResizeAndAnimate(gfx::Tween::Type tween_type,
                                               int target_width) {
  if (resize_animation_ && !toolbar_actions_bar_->suppress_animation()) {
    if (!ShownInsideMenu()) {
      // Make sure we don't try to animate to wider than the allowed width.
      int max_width = GetToolbarView(browser_)->GetMaxBrowserActionsWidth();
      if (target_width > max_width)
        target_width = GetWidthForMaxWidth(max_width);
    }
    // Animate! We have to set the animation_target_size_ after calling Reset(),
    // because that could end up calling AnimationEnded which clears the value.
    resize_animation_->Reset();
    resize_starting_width_ = width();
    resize_animation_->SetTweenType(tween_type);
    animation_target_size_ = target_width;
    resize_animation_->Show();
  } else {
    animation_target_size_ = target_width;
    AnimationEnded(resize_animation_.get());
  }
}

int BrowserActionsContainer::GetWidth(GetWidthTime get_width_time) const {
  return get_width_time == GET_WIDTH_AFTER_ANIMATION &&
                 animation_target_size_ > 0
             ? animation_target_size_
             : width();
}

bool BrowserActionsContainer::IsAnimating() const {
  return animating();
}

void BrowserActionsContainer::StopAnimating() {
  animation_target_size_ = width();
  resize_animation_->Reset();
}

void BrowserActionsContainer::ShowToolbarActionBubble(
    std::unique_ptr<ToolbarActionsBarBubbleDelegate> controller) {
  // The container shouldn't be asked to show a bubble if it's animating.
  DCHECK(!animating());
  DCHECK(!active_bubble_);

  views::View* anchor_view = nullptr;
  bool anchored_to_action_view = false;
  if (!controller->GetAnchorActionId().empty()) {
    ToolbarActionView* action_view =
        GetViewForId(controller->GetAnchorActionId());
    if (action_view) {
      anchor_view =
          action_view->visible() ? action_view : GetOverflowReferenceView();
      anchored_to_action_view = true;
    } else {
      anchor_view = BrowserView::GetBrowserViewForBrowser(browser_)
                        ->toolbar()
                        ->app_menu_button();
    }
  } else {
    anchor_view = this;
  }

  ToolbarActionsBarBubbleViews* bubble = new ToolbarActionsBarBubbleViews(
      anchor_view, gfx::Point(), anchored_to_action_view,
      std::move(controller));
  active_bubble_ = bubble;
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->GetWidget()->AddObserver(this);
  bubble->Show();
}

void BrowserActionsContainer::OnWidgetClosing(views::Widget* widget) {
  ClearActiveBubble(widget);
}

void BrowserActionsContainer::OnWidgetDestroying(views::Widget* widget) {
  ClearActiveBubble(widget);
}

int BrowserActionsContainer::GetWidthForMaxWidth(int max_width) const {
  int preferred_width = GetPreferredSize().width();
  if (preferred_width > max_width) {
    // If we can't even show the minimum width, just throw in the towel (and
    // show nothing).
    if (max_width < toolbar_actions_bar_->GetMinimumWidth())
      return 0;
    preferred_width = toolbar_actions_bar_->IconCountToWidth(
        toolbar_actions_bar_->WidthToIconCount(max_width));
  }
  return preferred_width;
}

gfx::Size BrowserActionsContainer::CalculatePreferredSize() const {
  if (ShownInsideMenu())
    return toolbar_actions_bar_->GetFullSize();

  // If there are no actions to show, then don't show the container at all.
  if (toolbar_action_views_.empty())
    return gfx::Size();

  // When resizing, preferred width is the starting width - resize amount.
  // Otherwise, use the normal preferred width.
  int preferred_width = resize_starting_width_ == -1
                            ? toolbar_actions_bar_->GetFullSize().width()
                            : resize_starting_width_ - resize_amount_;
  // In either case, clamp it within the max/min bounds.
  preferred_width = std::min(
      std::max(toolbar_actions_bar_->GetMinimumWidth(), preferred_width),
      toolbar_actions_bar_->GetMaximumWidth());
  return gfx::Size(preferred_width, ToolbarActionsBar::IconHeight());
}

int BrowserActionsContainer::GetHeightForWidth(int width) const {
  if (ShownInsideMenu())
    toolbar_actions_bar_->SetOverflowRowWidth(width);
  return GetPreferredSize().height();
}

gfx::Size BrowserActionsContainer::GetMinimumSize() const {
  return gfx::Size(toolbar_actions_bar_->GetMinimumWidth(),
                   ToolbarActionsBar::IconHeight());
}

void BrowserActionsContainer::Layout() {
  if (toolbar_actions_bar_->suppress_layout())
    return;

  if (toolbar_action_views_.empty()) {
    SetVisible(false);
    return;
  }

  SetVisible(true);
  if (resize_area_)
    resize_area_->SetBounds(0, 0, platform_settings().item_spacing, height());

  // The range of visible icons, from start_index (inclusive) to end_index
  // (exclusive).
  size_t start_index = toolbar_actions_bar_->GetStartIndexInBounds();
  size_t end_index = toolbar_actions_bar_->GetEndIndexInBounds();

  // Now draw the icons for the actions in the available space. Once all the
  // variables are in place, the layout works equally well for the main and
  // overflow container.
  for (size_t i = 0u; i < toolbar_action_views_.size(); ++i) {
    ToolbarActionView* view = toolbar_action_views_[i].get();
    if (i < start_index || i >= end_index) {
      view->SetVisible(false);
    } else {
      view->SetBoundsRect(toolbar_actions_bar_->GetFrameForIndex(i));
      view->SetVisible(true);
    }
  }
}

bool BrowserActionsContainer::GetDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  return BrowserActionDragData::GetDropFormats(format_types);
}

bool BrowserActionsContainer::AreDropTypesRequired() {
  return BrowserActionDragData::AreDropTypesRequired();
}

bool BrowserActionsContainer::CanDrop(const OSExchangeData& data) {
  return BrowserActionDragData::CanDrop(data, browser_->profile());
}

int BrowserActionsContainer::OnDragUpdated(
    const ui::DropTargetEvent& event) {
  size_t row_index = 0;
  size_t before_icon_in_row = 0;
  // If there are no visible actions (such as when dragging an icon to an empty
  // overflow/main container), then 0, 0 for row, column is correct.
  if (VisibleBrowserActions() != 0) {
    // Figure out where to display the indicator. This is a complex calculation:

    // First, we subtract out the padding to the left of the icon area. If
    // we're right-to-left, we also mirror the event.x() so that our
    // calculations are consistent with left-to-right.
    int offset_into_icon_area =
        GetMirroredXInView(event.x()) -
            GetLayoutConstant(TOOLBAR_STANDARD_SPACING);

    // Next, figure out what row we're on. This only matters for overflow mode,
    // but the calculation is the same for both.
    row_index = event.y() / ToolbarActionsBar::IconHeight();

    // Sanity check - we should never be on a different row in the main
    // container.
    DCHECK(ShownInsideMenu() || row_index == 0);

    // Next, we determine which icon to place the indicator in front of. We want
    // to place the indicator in front of icon n when the cursor is between the
    // midpoints of icons (n - 1) and n.  To do this we take the offset into the
    // icon area and transform it as follows:
    //
    // Real icon area:
    //   0   a     *  b        c
    //   |   |        |        |
    //   |[IC|ON]  [IC|ON]  [IC|ON]
    // We want to be before icon 0 for 0 < x <= a, icon 1 for a < x <= b, etc.
    // Here the "*" represents the offset into the icon area, and since it's
    // between a and b, we want to return "1".
    //
    // Transformed "icon area":
    //   0        a     *  b        c
    //   |        |        |        |
    //   |[ICON]  |[ICON]  |[ICON]  |
    // If we shift both our offset and our divider points later by half an icon
    // plus one spacing unit, then it becomes very easy to calculate how many
    // divider points we've passed, because they're the multiples of "one icon
    // plus padding".
    int before_icon_unclamped =
        (offset_into_icon_area + (ToolbarActionsBar::IconWidth(false) / 2) +
        platform_settings().item_spacing) / ToolbarActionsBar::IconWidth(true);

    // We need to figure out how many icons are visible on the relevant row.
    // In the main container, this will just be the visible actions.
    int visible_icons_on_row = VisibleBrowserActionsAfterAnimation();
    if (ShownInsideMenu()) {
      int icons_per_row = platform_settings().icons_per_overflow_menu_row;
      // If this is the final row of the overflow, then this is the remainder of
      // visible icons. Otherwise, it's a full row (kIconsPerRow).
      visible_icons_on_row =
          row_index ==
              static_cast<size_t>(visible_icons_on_row / icons_per_row) ?
                  visible_icons_on_row % icons_per_row : icons_per_row;
    }

    // Because the user can drag outside the container bounds, we need to clamp
    // to the valid range. Note that the maximum allowable value is (num icons),
    // not (num icons - 1), because we represent the indicator being past the
    // last icon as being "before the (last + 1) icon".
    before_icon_in_row =
        std::min(std::max(before_icon_unclamped, 0), visible_icons_on_row);
  }

  if (!drop_position_.get() ||
      !(drop_position_->row == row_index &&
        drop_position_->icon_in_row == before_icon_in_row)) {
    drop_position_.reset(new DropPosition(row_index, before_icon_in_row));
    SchedulePaint();
  }

  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::OnDragExited() {
  drop_position_.reset();
  SchedulePaint();
}

int BrowserActionsContainer::OnPerformDrop(
    const ui::DropTargetEvent& event) {
  BrowserActionDragData data;
  if (!data.Read(event.data()))
    return ui::DragDropTypes::DRAG_NONE;

  // Make sure we have the same view as we started with.
  DCHECK_EQ(GetIdAt(data.index()), data.id());

  size_t i = drop_position_->row *
      platform_settings().icons_per_overflow_menu_row +
      drop_position_->icon_in_row;
  if (ShownInsideMenu())
    i += main_container_->VisibleBrowserActionsAfterAnimation();
  // |i| now points to the item to the right of the drop indicator*, which is
  // correct when dragging an icon to the left. When dragging to the right,
  // however, we want the icon being dragged to get the index of the item to
  // the left of the drop indicator, so we subtract one.
  // * Well, it can also point to the end, but not when dragging to the left. :)
  if (i > data.index())
    --i;

  ToolbarActionsBar::DragType drag_type = ToolbarActionsBar::DRAG_TO_SAME;
  if (!toolbar_action_views_[data.index()]->visible())
    drag_type = ShownInsideMenu() ? ToolbarActionsBar::DRAG_TO_OVERFLOW :
        ToolbarActionsBar::DRAG_TO_MAIN;

  toolbar_actions_bar_->OnDragDrop(data.index(), i, drag_type);

  OnDragExited();  // Perform clean up after dragging.
  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_GROUP;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_EXTENSIONS));
}

void BrowserActionsContainer::WriteDragDataForView(View* sender,
                                                   const gfx::Point& press_pt,
                                                   OSExchangeData* data) {
  toolbar_actions_bar_->OnDragStarted();
  DCHECK(data);

  auto it =
      std::find_if(toolbar_action_views_.cbegin(), toolbar_action_views_.cend(),
                   [sender](const std::unique_ptr<ToolbarActionView>& ptr) {
                     return ptr.get() == sender;
                   });
  DCHECK(it != toolbar_action_views_.cend());
  ToolbarActionViewController* view_controller = (*it)->view_controller();
  gfx::Size size(ToolbarActionsBar::IconWidth(false),
                 ToolbarActionsBar::IconHeight());
  data->provider().SetDragImage(
      view_controller->GetIcon(GetCurrentWebContents(), size).AsImageSkia(),
      press_pt.OffsetFromOrigin());
  // Fill in the remaining info.
  BrowserActionDragData drag_data(view_controller->GetId(),
                                  it - toolbar_action_views_.cbegin());
  drag_data.Write(browser_->profile(), data);
}

int BrowserActionsContainer::GetDragOperationsForView(View* sender,
                                                      const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_MOVE;
}

bool BrowserActionsContainer::CanStartDragForView(View* sender,
                                                  const gfx::Point& press_pt,
                                                  const gfx::Point& p) {
  // We don't allow dragging while we're highlighting.
  return !toolbar_actions_bar_->is_highlighting();
}

void BrowserActionsContainer::OnResize(int resize_amount, bool done_resizing) {
  // We don't allow resize while the toolbar is highlighting a subset of
  // actions, since this is a temporary and entirely browser-driven sequence in
  // order to warn the user about potentially dangerous items.
  // We also don't allow resize when the bar is already animating, since we
  // don't want two competing size changes.
  if (toolbar_actions_bar_->is_highlighting() || animating())
    return;

  // If this is the start of the resize gesture, initialize the starting
  // width.
  if (resize_starting_width_ == -1)
    resize_starting_width_ = width();

  if (!done_resizing) {
    resize_amount_ = resize_amount;
    parent()->Layout();
    return;
  }

  // Up until now we've only been modifying the resize_amount, but now it is
  // time to set the container size to the size we have resized to, and then
  // animate to the nearest icon count size if necessary (which may be 0).
  int ending_width =
      std::min(std::max(toolbar_actions_bar_->GetMinimumWidth(),
                        resize_starting_width_ - resize_amount),
               toolbar_actions_bar_->GetMaximumWidth());
  resize_starting_width_ = -1;
  toolbar_actions_bar_->OnResizeComplete(ending_width);
}

void BrowserActionsContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(resize_animation_.get(), animation);
  resize_amount_ = static_cast<int>(resize_animation_->GetCurrentValue() *
      (resize_starting_width_ - animation_target_size_));
  parent()->Layout();
}

void BrowserActionsContainer::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void BrowserActionsContainer::AnimationEnded(const gfx::Animation* animation) {
  animation_target_size_ = 0;
  resize_amount_ = 0;
  resize_starting_width_ = -1;
  parent()->Layout();

  toolbar_actions_bar_->OnAnimationEnded();
}

content::WebContents* BrowserActionsContainer::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

void BrowserActionsContainer::OnPaint(gfx::Canvas* canvas) {
  // If the views haven't been initialized yet, wait for the next call to
  // paint (one will be triggered by entering highlight mode).
  if (toolbar_actions_bar_->is_highlighting() &&
      !toolbar_action_views_.empty() && !ShownInsideMenu()) {
    views::Painter::PaintPainterAt(canvas, warning_highlight_painter_.get(),
                                   GetLocalBounds());
  }

  // TODO(sky/glen): Instead of using a drop indicator, animate the icons while
  // dragging (like we do for tab dragging).
  if (drop_position_.get()) {
    // The two-pixel width drop indicator.
    static const int kDropIndicatorWidth = 2;

    // Convert back to a pixel offset into the container.  First find the X
    // coordinate of the drop icon.
    const int drop_icon_x = GetLayoutConstant(TOOLBAR_STANDARD_SPACING) +
        (drop_position_->icon_in_row * ToolbarActionsBar::IconWidth(true));
    // Next, find the space before the drop icon.
    const int space_before_drop_icon = platform_settings().item_spacing;
    // Now place the drop indicator halfway between this and the end of the
    // previous icon.  If there is an odd amount of available space between the
    // two icons (or the icon and the address bar) after subtracting the drop
    // indicator width, this calculation puts the extra pixel on the left side
    // of the indicator, since when the indicator is between the address bar and
    // the first icon, it looks better closer to the icon.
    const int drop_indicator_x = drop_icon_x -
        ((space_before_drop_icon + kDropIndicatorWidth) / 2);
    const int row_height = ToolbarActionsBar::IconHeight();
    const int drop_indicator_y = row_height * drop_position_->row;
    gfx::Rect indicator_bounds = GetMirroredRect(gfx::Rect(
        drop_indicator_x, drop_indicator_y, kDropIndicatorWidth, row_height));

    // Color of the drop indicator.
    static const SkColor kDropIndicatorColor = SK_ColorBLACK;
    canvas->FillRect(indicator_bounds, kDropIndicatorColor);
  }
}

void BrowserActionsContainer::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (!toolbar_actions_bar_->enabled())
    return;

  if (details.is_add && details.child == this) {
    // Initial toolbar button creation and placement in the widget hierarchy.
    // We do this here instead of in the constructor because adding views
    // calls Layout on the Toolbar, which needs this object to be constructed
    // before its Layout function is called.
    toolbar_actions_bar_->CreateActions();

    added_to_view_ = true;
  }
}

void BrowserActionsContainer::ClearActiveBubble(views::Widget* widget) {
  DCHECK(active_bubble_);
  DCHECK_EQ(active_bubble_->GetWidget(), widget);
  widget->RemoveObserver(this);
  active_bubble_ = nullptr;
  toolbar_actions_bar_->OnBubbleClosed();
}
