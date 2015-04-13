// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"

#include "base/compiler_specific.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/extensions/extension_toolbar_icon_surfacing_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container_observer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/wrench_toolbar_button.h"
#include "chrome/common/extensions/command.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/feature_switch.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace {

// Horizontal spacing before the chevron (if visible).
const int kChevronSpacing = ToolbarView::kStandardSpacing - 2;

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
          main_container ?
              main_container->toolbar_actions_bar_.get() : nullptr)),
      browser_(browser),
      main_container_(main_container),
      popup_owner_(NULL),
      container_width_(0),
      resize_area_(NULL),
      chevron_(NULL),
      suppress_chevron_(false),
      added_to_view_(false),
      shown_bubble_(false),
      resize_amount_(0),
      animation_target_size_(0) {
  set_id(VIEW_ID_BROWSER_ACTION_TOOLBAR);

  bool overflow_experiment =
      extensions::FeatureSwitch::extension_action_redesign()->IsEnabled();
  DCHECK(!in_overflow_mode() || overflow_experiment);

  if (!in_overflow_mode()) {
    resize_animation_.reset(new gfx::SlideAnimation(this));
    resize_area_ = new views::ResizeArea(this);
    AddChildView(resize_area_);

    // 'Main' mode doesn't need a chevron overflow when overflow is shown inside
    // the Chrome menu.
    if (!overflow_experiment) {
      // Since the ChevronMenuButton holds a raw pointer to us, we need to
      // ensure it doesn't outlive us.  Having it owned by the view hierarchy as
      // a child will suffice.
      chevron_ = new ChevronMenuButton(this);
      chevron_->EnableCanvasFlippingForRTLUI(true);
      chevron_->SetAccessibleName(
          l10n_util::GetStringUTF16(IDS_ACCNAME_EXTENSIONS_CHEVRON));
      chevron_->SetVisible(false);
      AddChildView(chevron_);
    }
  }
}

BrowserActionsContainer::~BrowserActionsContainer() {
  FOR_EACH_OBSERVER(BrowserActionsContainerObserver,
                    observers_,
                    OnBrowserActionsContainerDestroyed());

  toolbar_actions_bar_->DeleteActions();
  // All views should be removed as part of ToolbarActionsBar::DeleteActions().
  DCHECK(toolbar_action_views_.empty());
}

void BrowserActionsContainer::Init() {
  LoadImages();

  // We wait to set the container width until now so that the chevron images
  // will be loaded.  The width calculation needs to know the chevron size.
  container_width_ = toolbar_actions_bar_->GetPreferredSize().width();
}

const std::string& BrowserActionsContainer::GetIdAt(size_t index) const {
  return toolbar_action_views_[index]->view_controller()->GetId();
}

ToolbarActionView* BrowserActionsContainer::GetViewForId(
    const std::string& id) {
  for (ToolbarActionView* view : toolbar_action_views_) {
    if (view->view_controller()->GetId() == id)
      return view;
  }
  return nullptr;
}

void BrowserActionsContainer::RefreshToolbarActionViews() {
  toolbar_actions_bar_->Update();
}

size_t BrowserActionsContainer::VisibleBrowserActions() const {
  size_t visible_actions = 0;
  for (const ToolbarActionView* view : toolbar_action_views_) {
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

void BrowserActionsContainer::ExecuteExtensionCommand(
    const extensions::Extension* extension,
    const extensions::Command& command) {
  // Global commands are handled by the ExtensionCommandsGlobalRegistry
  // instance.
  DCHECK(!command.global());
  extension_keybinding_registry_->ExecuteCommand(extension->id(),
                                                 command.accelerator());
}

bool BrowserActionsContainer::ShownInsideMenu() const {
  return in_overflow_mode();
}

void BrowserActionsContainer::OnToolbarActionViewDragDone() {
  FOR_EACH_OBSERVER(BrowserActionsContainerObserver,
                    observers_,
                    OnBrowserActionDragDone());
}

views::MenuButton* BrowserActionsContainer::GetOverflowReferenceView() {
  // With traditional overflow, the reference is the chevron. With the
  // redesign, we use the wrench menu instead.
  return chevron_ ?
      static_cast<views::MenuButton*>(chevron_) :
      static_cast<views::MenuButton*>(BrowserView::GetBrowserViewForBrowser(
          browser_)->toolbar()->app_menu());
}

void BrowserActionsContainer::SetPopupOwner(ToolbarActionView* popup_owner) {
  // We should never be setting a popup owner when one already exists, and
  // never unsetting one when one wasn't set.
  DCHECK((!popup_owner_ && popup_owner) ||
         (popup_owner_ && !popup_owner));
  popup_owner_ = popup_owner;
}

void BrowserActionsContainer::HideActivePopup() {
  if (popup_owner_)
    popup_owner_->view_controller()->HidePopup();
}

ToolbarActionView* BrowserActionsContainer::GetMainViewForAction(
    ToolbarActionView* view) {
  if (!in_overflow_mode())
    return view;  // This is the main view.

  // The overflow container and main container each have the same views and
  // view indices, so we can return the view of the index that |view| has in
  // this container.
  ToolbarActionViews::const_iterator iter =
      std::find(toolbar_action_views_.begin(),
                toolbar_action_views_.end(),
                view);
  DCHECK(iter != toolbar_action_views_.end());
  size_t index = iter - toolbar_action_views_.begin();
  return main_container_->toolbar_action_views_[index];
}

void BrowserActionsContainer::AddViewForAction(
   ToolbarActionViewController* view_controller,
   size_t index) {
  if (chevron_)
    chevron_->CloseMenu();

      view_controller->GetActionName();
  ToolbarActionView* view =
      new ToolbarActionView(view_controller, browser_->profile(), this);
  toolbar_action_views_.insert(toolbar_action_views_.begin() + index, view);
  AddChildViewAt(view, index);
}

void BrowserActionsContainer::RemoveViewForAction(
    ToolbarActionViewController* action) {
  if (chevron_)
    chevron_->CloseMenu();

  for (ToolbarActionViews::iterator iter = toolbar_action_views_.begin();
       iter != toolbar_action_views_.end(); ++iter) {
    if ((*iter)->view_controller() == action) {
      delete *iter;
      toolbar_action_views_.erase(iter);
      break;
    }
  }
}

void BrowserActionsContainer::RemoveAllViews() {
  HideActivePopup();
  STLDeleteElements(&toolbar_action_views_);
}

void BrowserActionsContainer::Redraw(bool order_changed) {
  if (!added_to_view_) {
    // We don't want to redraw before the view has been fully added to the
    // hierarchy.
    return;
  }

  std::vector<ToolbarActionViewController*> actions =
      toolbar_actions_bar_->toolbar_actions();
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
      }
    }
  }

  if (width() != GetPreferredSize().width() && parent()) {
    parent()->Layout();
    parent()->SchedulePaint();
  } else {
    Layout();
    SchedulePaint();
  }
}

void BrowserActionsContainer::ResizeAndAnimate(
    gfx::Tween::Type tween_type,
    int target_width,
    bool suppress_chevron) {
  if (resize_animation_ && !toolbar_actions_bar_->suppress_animation()) {
    // Animate! We have to set the animation_target_size_ after calling Reset(),
    // because that could end up calling AnimationEnded which clears the value.
    resize_animation_->Reset();
    suppress_chevron_ = suppress_chevron;
    resize_animation_->SetTweenType(tween_type);
    animation_target_size_ = target_width;
    resize_animation_->Show();
  } else {
    animation_target_size_ = target_width;
    AnimationEnded(resize_animation_.get());
  }
}

void BrowserActionsContainer::SetChevronVisibility(bool visible) {
  if (chevron_)
    chevron_->SetVisible(visible);
}

int BrowserActionsContainer::GetWidth() const {
  return container_width_;
}

bool BrowserActionsContainer::IsAnimating() const {
  return animating();
}

void BrowserActionsContainer::StopAnimating() {
  animation_target_size_ = container_width_;
  resize_animation_->Reset();
}

int BrowserActionsContainer::GetChevronWidth() const {
  return chevron_ ? chevron_->GetPreferredSize().width() + kChevronSpacing : 0;
}

bool BrowserActionsContainer::IsPopupRunning() const {
  return popup_owner_ != nullptr;
}

void BrowserActionsContainer::OnOverflowedActionWantsToRunChanged(
    bool overflowed_action_wants_to_run) {
  DCHECK(!in_overflow_mode());
  BrowserView::GetBrowserViewForBrowser(browser_)->toolbar()->
      app_menu()->SetOverflowedToolbarActionWantsToRun(
          overflowed_action_wants_to_run);
}

void BrowserActionsContainer::AddObserver(
    BrowserActionsContainerObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserActionsContainer::RemoveObserver(
    BrowserActionsContainerObserver* observer) {
  observers_.RemoveObserver(observer);
}

gfx::Size BrowserActionsContainer::GetPreferredSize() const {
  if (in_overflow_mode())
    return toolbar_actions_bar_->GetPreferredSize();

  // If there are no actions to show, then don't show the container at all.
  if (toolbar_action_views_.empty())
    return gfx::Size();

  // We calculate the size of the view by taking the current width and
  // subtracting resize_amount_ (the latter represents how far the user is
  // resizing the view or, if animating the snapping, how far to animate it).
  // But we also clamp it to a minimum size and the maximum size, so that the
  // container can never shrink too far or take up more space than it needs.
  // In other words: minimum_width < width - resize < max_width.
  int preferred_width = std::min(
      std::max(toolbar_actions_bar_->GetMinimumWidth(),
               container_width_ - resize_amount_),
      toolbar_actions_bar_->GetMaximumWidth());
  return gfx::Size(preferred_width, ToolbarActionsBar::IconHeight());
}

int BrowserActionsContainer::GetHeightForWidth(int width) const {
  if (in_overflow_mode())
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

  // If the icons don't all fit, show the chevron (unless suppressed).
  int max_x = GetPreferredSize().width();
  if (toolbar_actions_bar_->IconCountToWidth(-1) > max_x &&
      !suppress_chevron_ && chevron_) {
    chevron_->SetVisible(true);
    gfx::Size chevron_size(chevron_->GetPreferredSize());
    max_x -= chevron_size.width() + kChevronSpacing;
    chevron_->SetBounds(
        width() - ToolbarView::kStandardSpacing - chevron_size.width(),
        0,
        chevron_size.width(),
        chevron_size.height());
  } else if (chevron_) {
    chevron_->SetVisible(false);
  }

  // The range of visible icons, from start_index (inclusive) to end_index
  // (exclusive).
  size_t start_index = in_overflow_mode() ?
      main_container_->toolbar_actions_bar_->GetIconCount() : 0u;
  // For the main container's last visible icon, we calculate how many icons we
  // can display with the given width. We add an extra item_spacing because the
  // last icon doesn't need padding, but we want it to divide easily.
  size_t end_index = in_overflow_mode() ?
      toolbar_action_views_.size() :
      (max_x - platform_settings().left_padding -
          platform_settings().right_padding +
          platform_settings().item_spacing) /
          ToolbarActionsBar::IconWidth(true);
  // The maximum length for one row of icons.
  size_t row_length = in_overflow_mode() ?
      platform_settings().icons_per_overflow_menu_row : end_index;

  // Now draw the icons for the actions in the available space. Once all the
  // variables are in place, the layout works equally well for the main and
  // overflow container.
  for (size_t i = 0u; i < toolbar_action_views_.size(); ++i) {
    ToolbarActionView* view = toolbar_action_views_[i];
    if (i < start_index || i >= end_index) {
      view->SetVisible(false);
    } else {
      size_t relative_index = i - start_index;
      size_t index_in_row = relative_index % row_length;
      size_t row_index = relative_index / row_length;
      view->SetBounds(platform_settings().left_padding +
                          index_in_row * ToolbarActionsBar::IconWidth(true),
                      row_index * ToolbarActionsBar::IconHeight(),
                      ToolbarActionsBar::IconWidth(false),
                      ToolbarActionsBar::IconHeight());
      view->SetVisible(true);
    }
  }
}

void BrowserActionsContainer::OnMouseEntered(const ui::MouseEvent& event) {
  if (!shown_bubble_ && !toolbar_action_views_.empty() &&
      toolbar_actions_bar_->ShouldShowInfoBubble()) {
    ExtensionToolbarIconSurfacingBubble* bubble =
        new ExtensionToolbarIconSurfacingBubble(toolbar_action_views_[0],
                                                toolbar_actions_bar_.get());
    views::BubbleDelegateView::CreateBubble(bubble);
    bubble->GetWidget()->Show();
  }
  shown_bubble_ = true;
}

bool BrowserActionsContainer::GetDropFormats(
    int* formats,
    std::set<OSExchangeData::CustomFormat>* custom_formats) {
  return BrowserActionDragData::GetDropFormats(custom_formats);
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

    // First, we subtract out the padding to the left of the icon area, which is
    // ToolbarView::kStandardSpacing. If we're right-to-left, we also mirror the
    // event.x() so that our calculations are consistent with left-to-right.
    int offset_into_icon_area =
        GetMirroredXInView(event.x()) - ToolbarView::kStandardSpacing;

    // Next, figure out what row we're on. This only matters for overflow mode,
    // but the calculation is the same for both.
    row_index = event.y() / ToolbarActionsBar::IconHeight();

    // Sanity check - we should never be on a different row in the main
    // container.
    DCHECK(in_overflow_mode() || row_index == 0);

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
    if (in_overflow_mode()) {
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
  if (in_overflow_mode())
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
    drag_type = in_overflow_mode() ? ToolbarActionsBar::DRAG_TO_OVERFLOW :
        ToolbarActionsBar::DRAG_TO_MAIN;

  toolbar_actions_bar_->OnDragDrop(data.index(), i, drag_type);

  OnDragExited();  // Perform clean up after dragging.
  return ui::DragDropTypes::DRAG_MOVE;
}

void BrowserActionsContainer::GetAccessibleState(
    ui::AXViewState* state) {
  state->role = ui::AX_ROLE_GROUP;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_EXTENSIONS);
}

void BrowserActionsContainer::WriteDragDataForView(View* sender,
                                                   const gfx::Point& press_pt,
                                                   OSExchangeData* data) {
  DCHECK(data);

  ToolbarActionViews::iterator iter = std::find(toolbar_action_views_.begin(),
                                                toolbar_action_views_.end(),
                                                sender);
  DCHECK(iter != toolbar_action_views_.end());
  ToolbarActionViewController* view_controller = (*iter)->view_controller();
  drag_utils::SetDragImageOnDataObject(
      view_controller->GetIconWithBadge(),
      press_pt.OffsetFromOrigin(),
      data);
  // Fill in the remaining info.
  BrowserActionDragData drag_data(view_controller->GetId(),
                                  iter - toolbar_action_views_.begin());
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
  if (!done_resizing) {
    resize_amount_ = resize_amount;
    Redraw(false);
    return;
  }

  // Up until now we've only been modifying the resize_amount, but now it is
  // time to set the container size to the size we have resized to, and then
  // animate to the nearest icon count size if necessary (which may be 0).
  container_width_ =
      std::min(std::max(toolbar_actions_bar_->GetMinimumWidth(),
                        container_width_ - resize_amount),
               toolbar_actions_bar_->GetMaximumWidth());
  toolbar_actions_bar_->OnResizeComplete(container_width_);
}

void BrowserActionsContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(resize_animation_.get(), animation);
  resize_amount_ = static_cast<int>(resize_animation_->GetCurrentValue() *
      (container_width_ - animation_target_size_));
  Redraw(false);
}

void BrowserActionsContainer::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void BrowserActionsContainer::AnimationEnded(const gfx::Animation* animation) {
  container_width_ = animation_target_size_;
  animation_target_size_ = 0;
  resize_amount_ = 0;
  suppress_chevron_ = false;
  Redraw(false);
  FOR_EACH_OBSERVER(BrowserActionsContainerObserver,
                    observers_,
                    OnBrowserActionsContainerAnimationEnded());
}

content::WebContents* BrowserActionsContainer::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

extensions::ActiveTabPermissionGranter*
    BrowserActionsContainer::GetActiveTabPermissionGranter() {
  content::WebContents* web_contents = GetCurrentWebContents();
  if (!web_contents)
    return NULL;
  return extensions::TabHelper::FromWebContents(web_contents)->
      active_tab_permission_granter();
}

gfx::NativeView BrowserActionsContainer::TestGetPopup() {
  return popup_owner_ ?
      popup_owner_->view_controller()->GetPopupNativeView() :
      NULL;
}

void BrowserActionsContainer::OnPaint(gfx::Canvas* canvas) {
  // If the views haven't been initialized yet, wait for the next call to
  // paint (one will be triggered by entering highlight mode).
  if (toolbar_actions_bar_->is_highlighting() &&
      !toolbar_action_views_.empty() &&
      highlight_painter_) {
    views::Painter::PaintPainterAt(
        canvas, highlight_painter_.get(), GetLocalBounds());
  }

  // TODO(sky/glen): Instead of using a drop indicator, animate the icons while
  // dragging (like we do for tab dragging).
  if (drop_position_.get()) {
    // The two-pixel width drop indicator.
    static const int kDropIndicatorWidth = 2;

    // Convert back to a pixel offset into the container.  First find the X
    // coordinate of the drop icon.
    int drop_icon_x = ToolbarView::kStandardSpacing +
        (drop_position_->icon_in_row * ToolbarActionsBar::IconWidth(true));
    // Next, find the space before the drop icon. This will either be
    // left padding or item spacing, depending on whether this is the first
    // icon.
    // NOTE: Right now, these are the same. But let's do this right for if they
    // ever aren't.
    int space_before_drop_icon = drop_position_->icon_in_row == 0 ?
        platform_settings().left_padding : platform_settings().item_spacing;
    // Now place the drop indicator halfway between this and the end of the
    // previous icon.  If there is an odd amount of available space between the
    // two icons (or the icon and the address bar) after subtracting the drop
    // indicator width, this calculation puts the extra pixel on the left side
    // of the indicator, since when the indicator is between the address bar and
    // the first icon, it looks better closer to the icon.
    int drop_indicator_x = drop_icon_x -
        ((space_before_drop_icon + kDropIndicatorWidth) / 2);
    int row_height = ToolbarActionsBar::IconHeight();
    int drop_indicator_y = row_height * drop_position_->row;
    gfx::Rect indicator_bounds(drop_indicator_x,
                               drop_indicator_y,
                               kDropIndicatorWidth,
                               row_height);
    indicator_bounds.set_x(GetMirroredXForRect(indicator_bounds));

    // Color of the drop indicator.
    static const SkColor kDropIndicatorColor = SK_ColorBLACK;
    canvas->FillRect(indicator_bounds, kDropIndicatorColor);
  }
}

void BrowserActionsContainer::OnThemeChanged() {
  LoadImages();
}

void BrowserActionsContainer::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (!toolbar_actions_bar_->enabled())
    return;

  if (details.is_add && details.child == this) {
    if (!in_overflow_mode() &&  // We only need one keybinding registry.
        parent()->GetFocusManager()) {  // focus manager can be null in tests.
      extension_keybinding_registry_.reset(new ExtensionKeybindingRegistryViews(
          browser_->profile(),
          parent()->GetFocusManager(),
          extensions::ExtensionKeybindingRegistry::ALL_EXTENSIONS,
          this));
    }

    // Initial toolbar button creation and placement in the widget hierarchy.
    // We do this here instead of in the constructor because adding views
    // calls Layout on the Toolbar, which needs this object to be constructed
    // before its Layout function is called.
    toolbar_actions_bar_->CreateActions();

    added_to_view_ = true;
  }
}

void BrowserActionsContainer::LoadImages() {
  if (in_overflow_mode())
    return;  // Overflow mode has neither a chevron nor highlighting.

  ui::ThemeProvider* tp = GetThemeProvider();
  if (tp && chevron_) {
    chevron_->SetImage(views::Button::STATE_NORMAL,
                       *tp->GetImageSkiaNamed(IDR_BROWSER_ACTIONS_OVERFLOW));
  }

  const int kImages[] = IMAGE_GRID(IDR_DEVELOPER_MODE_HIGHLIGHT);
  highlight_painter_.reset(views::Painter::CreateImageGridPainter(kImages));
}
