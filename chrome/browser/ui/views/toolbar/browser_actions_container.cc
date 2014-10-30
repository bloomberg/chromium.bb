// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"

#include "base/compiler_specific.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/browser_action_drag_data.h"
#include "chrome/browser/ui/views/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container_observer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/command.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/feature_switch.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/nine_image_painter_factory.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

using extensions::Extension;

namespace {

// Horizontal spacing before the chevron (if visible).
const int kChevronSpacing = ToolbarView::kStandardSpacing - 2;

// Returns the set of controllers for all actions.
// TODO(devlin): We should move this to the model, once it supports component
// actions.
ScopedVector<ToolbarActionViewController> GetToolbarActions(
    extensions::ExtensionToolbarModel* model,
    Browser* browser) {
  ScopedVector<ToolbarActionViewController> actions;

  // Extension actions come first.
  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(browser->profile());
  const extensions::ExtensionList& toolbar_items = model->toolbar_items();
  for (const scoped_refptr<const Extension>& extension : toolbar_items) {
    actions.push_back(new ExtensionActionViewController(
        extension.get(),
        browser,
        action_manager->GetExtensionAction(*extension)));
  }

  // Component actions come second.
  ScopedVector<ToolbarActionViewController> component_actions =
      ComponentToolbarActionsFactory::GetInstance()->
          GetComponentToolbarActions();
  DCHECK(extensions::FeatureSwitch::extension_action_redesign()->IsEnabled() ||
         component_actions.empty());
  actions.insert(
      actions.end(), component_actions.begin(), component_actions.end());
  component_actions.weak_clear();
  return actions.Pass();
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

// static
int BrowserActionsContainer::icons_per_overflow_menu_row_ = 1;

// static
const int BrowserActionsContainer::kItemSpacing = ToolbarView::kStandardSpacing;

// static
bool BrowserActionsContainer::disable_animations_during_testing_ = false;

BrowserActionsContainer::BrowserActionsContainer(
    Browser* browser,
    BrowserActionsContainer* main_container)
    : initialized_(false),
      profile_(browser->profile()),
      browser_(browser),
      main_container_(main_container),
      popup_owner_(NULL),
      model_(extensions::ExtensionToolbarModel::Get(browser->profile())),
      container_width_(0),
      resize_area_(NULL),
      chevron_(NULL),
      suppress_chevron_(false),
      resize_amount_(0),
      animation_target_size_(0) {
  set_id(VIEW_ID_BROWSER_ACTION_TOOLBAR);
  if (model_)  // |model_| can be NULL in views unittests.
    model_->AddObserver(this);

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

  if (model_)
    model_->RemoveObserver(this);
  HideActivePopup();
  DeleteToolbarActionViews();
}

void BrowserActionsContainer::Init() {
  LoadImages();

  // We wait to set the container width until now so that the chevron images
  // will be loaded.  The width calculation needs to know the chevron size.
  if (model_ && model_->extensions_initialized()) {
    container_width_ = GetPreferredWidth();
    SetChevronVisibility();
  }

  initialized_ = true;
}

const std::string& BrowserActionsContainer::GetIdAt(size_t index) {
  return toolbar_action_views_[index]->view_controller()->GetId();
}

ToolbarActionView* BrowserActionsContainer::GetViewForExtension(
    const Extension* extension) {
  for (ToolbarActionView* view : toolbar_action_views_) {
    if (view->view_controller()->GetId() == extension->id())
      return view;
  }
  return nullptr;
}

void BrowserActionsContainer::RefreshToolbarActionViews() {
  for (ToolbarActionView* view : toolbar_action_views_)
    view->UpdateState();
}

void BrowserActionsContainer::CreateToolbarActionViews() {
  DCHECK(toolbar_action_views_.empty());
  if (!model_)
    return;

  ScopedVector<ToolbarActionViewController> actions =
      GetToolbarActions(model_, browser_);
  for (ToolbarActionViewController* controller : actions) {
    ToolbarActionView* view =
        new ToolbarActionView(make_scoped_ptr(controller), browser_, this);
    toolbar_action_views_.push_back(view);
    AddChildView(view);
  }
  actions.weak_clear();
}

void BrowserActionsContainer::DeleteToolbarActionViews() {
  HideActivePopup();
  STLDeleteElements(&toolbar_action_views_);
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

  return WidthToIconCount(animation_target_size_);
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

void BrowserActionsContainer::NotifyActionMovedToOverflow() {
  // When an action is moved to overflow, we shrink the size of the container
  // by 1.
  int icon_count = model_->GetVisibleIconCount();
  // Since this happens when an icon moves from the main bar to overflow, we
  // can't possibly have had no visible icons on the main bar.
  DCHECK_NE(0, icon_count);
  if (icon_count == -1)
    icon_count = toolbar_action_views_.size();
  model_->SetVisibleIconCount(icon_count - 1);
}

bool BrowserActionsContainer::ShownInsideMenu() const {
  return in_overflow_mode();
}

void BrowserActionsContainer::OnToolbarActionViewDragDone() {
  ToolbarVisibleCountChanged();
  FOR_EACH_OBSERVER(BrowserActionsContainerObserver,
                    observers_,
                    OnBrowserActionDragDone());
}

views::MenuButton* BrowserActionsContainer::GetOverflowReferenceView() {
  // With traditional overflow, the reference is the chevron. With the
  // redesign, we use the wrench menu instead.
  return chevron_ ?
      chevron_ :
      BrowserView::GetBrowserViewForBrowser(browser_)->toolbar()->app_menu();
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

void BrowserActionsContainer::AddObserver(
    BrowserActionsContainerObserver* observer) {
  observers_.AddObserver(observer);
}

void BrowserActionsContainer::RemoveObserver(
    BrowserActionsContainerObserver* observer) {
  observers_.RemoveObserver(observer);
}

gfx::Size BrowserActionsContainer::GetPreferredSize() const {
  if (in_overflow_mode()) {
    int icon_count = GetIconCount();
    // In overflow, we always have a preferred size of a full row (even if we
    // don't use it), and always of at least one row. The parent may decide to
    // show us even when empty, e.g. as a drag target for dragging in icons from
    // the main container.
    int row_count =
        ((std::max(0, icon_count - 1)) / icons_per_overflow_menu_row_) + 1;
    return gfx::Size(IconCountToWidth(icons_per_overflow_menu_row_),
                     row_count * IconHeight());
  }

  // If there are no actions to show, then don't show the container at all.
  if (toolbar_action_views_.empty())
    return gfx::Size();

  // We calculate the size of the view by taking the current width and
  // subtracting resize_amount_ (the latter represents how far the user is
  // resizing the view or, if animating the snapping, how far to animate it).
  // But we also clamp it to a minimum size and the maximum size, so that the
  // container can never shrink too far or take up more space than it needs.
  // In other words: MinimumNonemptyWidth() < width() - resize < ClampTo(MAX).
  int preferred_width = std::min(
      std::max(MinimumNonemptyWidth(), container_width_ - resize_amount_),
      IconCountToWidth(-1));
  return gfx::Size(preferred_width, IconHeight());
}

int BrowserActionsContainer::GetHeightForWidth(int width) const {
  if (in_overflow_mode())
    icons_per_overflow_menu_row_ = (width - kItemSpacing) / IconWidth(true);
  return GetPreferredSize().height();
}

gfx::Size BrowserActionsContainer::GetMinimumSize() const {
  int min_width = std::min(MinimumNonemptyWidth(), IconCountToWidth(-1));
  return gfx::Size(min_width, IconHeight());
}

void BrowserActionsContainer::Layout() {
  if (toolbar_action_views_.empty()) {
    SetVisible(false);
    return;
  }

  SetVisible(true);
  if (resize_area_)
    resize_area_->SetBounds(0, 0, kItemSpacing, height());

  // If the icons don't all fit, show the chevron (unless suppressed).
  int max_x = GetPreferredSize().width();
  if (IconCountToWidth(-1) > max_x && !suppress_chevron_ && chevron_) {
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

  // The padding before the first icon and after the last icon in the container.
  int container_padding =
      in_overflow_mode() ? kItemSpacing : ToolbarView::kStandardSpacing;
  // The range of visible icons, from start_index (inclusive) to end_index
  // (exclusive).
  size_t start_index = in_overflow_mode() ?
      main_container_->VisibleBrowserActionsAfterAnimation() : 0u;
  // For the main container's last visible icon, we calculate how many icons we
  // can display with the given width. We add an extra kItemSpacing because the
  // last icon doesn't need padding, but we want it to divide easily.
  size_t end_index = in_overflow_mode() ?
      toolbar_action_views_.size() :
      (max_x - 2 * container_padding + kItemSpacing) / IconWidth(true);
  // The maximum length for one row of icons.
  size_t row_length =
      in_overflow_mode() ? icons_per_overflow_menu_row_ : end_index;

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
      view->SetBounds(container_padding + index_in_row * IconWidth(true),
                      row_index * IconHeight(),
                      IconWidth(false),
                      IconHeight());
      view->SetVisible(true);
    }
  }
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
  return BrowserActionDragData::CanDrop(data, profile_);
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
    row_index = event.y() / IconHeight();

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
        (offset_into_icon_area + (IconWidth(false) / 2) +
        kItemSpacing) / IconWidth(true);

    // We need to figure out how many icons are visible on the relevant row.
    // In the main container, this will just be the visible actions.
    int visible_icons_on_row = VisibleBrowserActionsAfterAnimation();
    if (in_overflow_mode()) {
      // If this is the final row of the overflow, then this is the remainder of
      // visible icons. Otherwise, it's a full row (kIconsPerRow).
      visible_icons_on_row =
          row_index ==
              static_cast<size_t>(visible_icons_on_row /
                                  icons_per_overflow_menu_row_) ?
                  visible_icons_on_row % icons_per_overflow_menu_row_ :
                  icons_per_overflow_menu_row_;
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
  DCHECK(model_);

  size_t i = drop_position_->row * icons_per_overflow_menu_row_ +
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

  // If this was a drag between containers, we will have to adjust the number of
  // visible icons.
  bool drag_between_containers =
      !toolbar_action_views_[data.index()]->visible();
  model_->MoveExtensionIcon(GetIdAt(data.index()), i);

  if (drag_between_containers) {
    // Let the main container update the model.
    if (in_overflow_mode())
      main_container_->NotifyActionMovedToOverflow();
    else  // This is the main container.
      model_->SetVisibleIconCount(model_->GetVisibleIconCount() + 1);
  }

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
  drag_data.Write(profile_, data);
}

int BrowserActionsContainer::GetDragOperationsForView(View* sender,
                                                      const gfx::Point& p) {
  return ui::DragDropTypes::DRAG_MOVE;
}

bool BrowserActionsContainer::CanStartDragForView(View* sender,
                                                  const gfx::Point& press_pt,
                                                  const gfx::Point& p) {
  // We don't allow dragging while we're highlighting.
  return !model_->is_highlighting();
}

void BrowserActionsContainer::OnResize(int resize_amount, bool done_resizing) {
  if (!done_resizing) {
    resize_amount_ = resize_amount;
    OnBrowserActionVisibilityChanged();
    return;
  }

  // Up until now we've only been modifying the resize_amount, but now it is
  // time to set the container size to the size we have resized to, and then
  // animate to the nearest icon count size if necessary (which may be 0).
  int max_width = IconCountToWidth(-1);
  container_width_ =
      std::min(std::max(0, container_width_ - resize_amount), max_width);

  // Save off the desired number of visible icons.  We do this now instead of at
  // the end of the animation so that even if the browser is shut down while
  // animating, the right value will be restored on next run.
  int visible_icons = WidthToIconCount(container_width_);
  model_->SetVisibleIconCount(visible_icons);
}

void BrowserActionsContainer::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(resize_animation_.get(), animation);
  resize_amount_ = static_cast<int>(resize_animation_->GetCurrentValue() *
      (container_width_ - animation_target_size_));
  OnBrowserActionVisibilityChanged();
}

void BrowserActionsContainer::AnimationEnded(const gfx::Animation* animation) {
  container_width_ = animation_target_size_;
  animation_target_size_ = 0;
  resize_amount_ = 0;
  suppress_chevron_ = false;
  SetChevronVisibility();
  OnBrowserActionVisibilityChanged();

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
  if (model_->is_highlighting() && !toolbar_action_views_.empty() &&
      !in_overflow_mode()) {
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
        (drop_position_->icon_in_row * IconWidth(true));
    // Next, find the space before the drop icon. This will either be
    // kItemSpacing or ToolbarView::kStandardSpacing, depending on whether this
    // is the first icon.
    // NOTE: Right now, these are the same. But let's do this right for if they
    // ever aren't.
    int space_before_drop_icon = drop_position_->icon_in_row == 0 ?
        ToolbarView::kStandardSpacing : kItemSpacing;
    // Now place the drop indicator halfway between this and the end of the
    // previous icon.  If there is an odd amount of available space between the
    // two icons (or the icon and the address bar) after subtracting the drop
    // indicator width, this calculation puts the extra pixel on the left side
    // of the indicator, since when the indicator is between the address bar and
    // the first icon, it looks better closer to the icon.
    int drop_indicator_x = drop_icon_x -
        ((space_before_drop_icon + kDropIndicatorWidth) / 2);
    int row_height = IconHeight();
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
  if (!model_)
    return;

  if (details.is_add && details.child == this) {
    if (!in_overflow_mode()) {  // We only need one keybinding registry.
      extension_keybinding_registry_.reset(new ExtensionKeybindingRegistryViews(
          browser_->profile(),
          parent()->GetFocusManager(),
          extensions::ExtensionKeybindingRegistry::ALL_EXTENSIONS,
          this));
    }

    // Initial toolbar button creation and placement in the widget hierarchy.
    // We do this here instead of in the constructor because AddBrowserAction
    // calls Layout on the Toolbar, which needs this object to be constructed
    // before its Layout function is called.
    CreateToolbarActionViews();
  }
}

// static
int BrowserActionsContainer::IconWidth(bool include_padding) {
  static bool initialized = false;
  static int icon_width = 0;
  if (!initialized) {
    initialized = true;
    icon_width = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_BROWSER_ACTION)->width();
  }
  return icon_width + (include_padding ? kItemSpacing : 0);
}

// static
int BrowserActionsContainer::IconHeight() {
  static bool initialized = false;
  static int icon_height = 0;
  if (!initialized) {
    initialized = true;
    icon_height = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_BROWSER_ACTION)->height();
  }
  return icon_height;
}

void BrowserActionsContainer::ToolbarExtensionAdded(const Extension* extension,
                                                    int index) {
  DCHECK(!GetViewForExtension(extension)) <<
      "Asked to add a browser action view for an extension that already exists";
  if (chevron_)
    chevron_->CloseMenu();

  // Add the new action to the vector and the view hierarchy.
  ToolbarActionView* view = new ToolbarActionView(
      make_scoped_ptr(new ExtensionActionViewController(
          extension,
          browser_,
          extensions::ExtensionActionManager::Get(profile_)->
              GetExtensionAction(*extension))),
      browser_,
      this);
  toolbar_action_views_.insert(toolbar_action_views_.begin() + index, view);
  AddChildViewAt(view, index);

  // If we are still initializing the container, don't bother animating.
  if (!model_->extensions_initialized())
    return;

  // If this is just an upgrade, then don't worry about resizing.
  if (!extensions::ExtensionSystem::Get(profile_)->runtime_data()->
          IsBeingUpgraded(extension)) {
    // We need to resize if either:
    // - The container is set to display all icons (visible count = -1), or
    // - The container will need to expand to include the chevron. This can
    //   happen when the container is set to display <n> icons, where <n> is
    //   the number of icons before the new icon. With the new icon, the chevron
    //   will need to be displayed.
    int model_icon_count = model_->GetVisibleIconCount();
    if (model_icon_count == -1 ||
        (static_cast<size_t>(model_icon_count) < toolbar_action_views_.size() &&
         (chevron_ && !chevron_->visible()))) {
      suppress_chevron_ = true;
      Animate(gfx::Tween::LINEAR, GetIconCount());
      return;
    }
  }

  // Otherwise, we don't have to resize, so just redraw the (possibly modified)
  // visible icon set.
  OnBrowserActionVisibilityChanged();
}

void BrowserActionsContainer::ToolbarExtensionRemoved(
    const Extension* extension) {
  if (chevron_)
    chevron_->CloseMenu();

  size_t visible_actions = VisibleBrowserActionsAfterAnimation();
  for (ToolbarActionViews::iterator i(toolbar_action_views_.begin());
       i != toolbar_action_views_.end(); ++i) {
    if ((*i)->view_controller()->GetId() == extension->id()) {
      delete *i;
      toolbar_action_views_.erase(i);

      // If the extension is being upgraded we don't want the bar to shrink
      // because the icon is just going to get re-added to the same location.
      if (extensions::ExtensionSystem::Get(profile_)->runtime_data()->
              IsBeingUpgraded(extension))
        return;

      if (toolbar_action_views_.size() > visible_actions) {
        // If we have more icons than we can show, then we must not be changing
        // the container size (since we either removed an icon from the main
        // area and one from the overflow list will have shifted in, or we
        // removed an entry directly from the overflow list).
        OnBrowserActionVisibilityChanged();
      } else {
        // Either we went from overflow to no-overflow, or we shrunk the no-
        // overflow container by 1.  Either way the size changed, so animate.
        if (chevron_)
          chevron_->SetVisible(false);
        Animate(gfx::Tween::EASE_OUT, toolbar_action_views_.size());
      }
      return;  // We have found the action to remove, bail out.
    }
  }
}

void BrowserActionsContainer::ToolbarExtensionMoved(const Extension* extension,
                                                    int index) {
  DCHECK(index >= 0 && index < static_cast<int>(toolbar_action_views_.size()));

  ToolbarActionViews::iterator iter = toolbar_action_views_.begin();
  while (iter != toolbar_action_views_.end() &&
         (*iter)->view_controller()->GetId() != extension->id())
    ++iter;

  DCHECK(iter != toolbar_action_views_.end());
  if (iter - toolbar_action_views_.begin() == index)
    return;  // Already in place.

  ToolbarActionView* moved_view = *iter;
  toolbar_action_views_.erase(iter);
  toolbar_action_views_.insert(
      toolbar_action_views_.begin() + index, moved_view);

  Layout();
  SchedulePaint();
}

void BrowserActionsContainer::ToolbarExtensionUpdated(
    const Extension* extension) {
  ToolbarActionView* view = GetViewForExtension(extension);
  // There might not be a view in cases where we are highlighting or if we
  // haven't fully initialized extensions.
  if (view)
    view->UpdateState();
}

bool BrowserActionsContainer::ShowExtensionActionPopup(
    const Extension* extension,
    bool grant_active_tab) {
  // Don't override another popup, and only show in the active window.
  if (popup_owner_ || !browser_->window()->IsActive())
    return false;

  ToolbarActionView* view = GetViewForExtension(extension);
  return view && view->view_controller()->ExecuteAction(grant_active_tab);
}

void BrowserActionsContainer::ToolbarVisibleCountChanged() {
  if (GetPreferredWidth() != container_width_)
    Animate(gfx::Tween::EASE_OUT, GetIconCount());
}

void BrowserActionsContainer::ToolbarHighlightModeChanged(
    bool is_highlighting) {
  // The visual highlighting is done in OnPaint(). It's a bit of a pain that
  // we delete and recreate everything here, but given everything else going on
  // (the lack of highlight, n more extensions appearing, etc), it's not worth
  // the extra complexity to create and insert only the new extensions.
  DeleteToolbarActionViews();
  CreateToolbarActionViews();
  Animate(gfx::Tween::LINEAR, GetIconCount());
}

Browser* BrowserActionsContainer::GetBrowser() {
  return browser_;
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

void BrowserActionsContainer::OnBrowserActionVisibilityChanged() {
  SetVisible(!toolbar_action_views_.empty());
  if (parent()) {  // Parent can be null in testing.
    parent()->Layout();
    parent()->SchedulePaint();
  }
}

int BrowserActionsContainer::GetPreferredWidth() {
  return IconCountToWidth(GetIconCount());
}

void BrowserActionsContainer::SetChevronVisibility() {
  if (chevron_) {
    chevron_->SetVisible(
        VisibleBrowserActionsAfterAnimation() < toolbar_action_views_.size());
  }
}

int BrowserActionsContainer::IconCountToWidth(int icons) const {
  if (icons < 0)
    icons = toolbar_action_views_.size();
  bool display_chevron =
      chevron_ && static_cast<size_t>(icons) < toolbar_action_views_.size();
  if (icons == 0 && !display_chevron)
    return ToolbarView::kStandardSpacing;
  int icons_size =
      (icons == 0) ? 0 : ((icons * IconWidth(true)) - kItemSpacing);
  int chevron_size = display_chevron ?
      (kChevronSpacing + chevron_->GetPreferredSize().width()) : 0;
  // In overflow mode, our padding is to use item spacing on either end (just so
  // we can see the drop indicator). Otherwise we use the standard toolbar
  // spacing.
  // Note: These are actually the same thing, but, on the offchance one
  // changes, let's get it right.
  int padding =
      2 * (in_overflow_mode() ? kItemSpacing : ToolbarView::kStandardSpacing);
  return icons_size + chevron_size + padding;
}

size_t BrowserActionsContainer::WidthToIconCount(int pixels) const {
  // Check for widths large enough to show the entire icon set.
  if (pixels >= IconCountToWidth(-1))
    return toolbar_action_views_.size();

  // We reserve space for the padding on either side of the toolbar...
  int available_space = pixels - (ToolbarView::kStandardSpacing * 2);
  // ... and, if the chevron is enabled, the chevron.
  if (chevron_)
    available_space -= (chevron_->GetPreferredSize().width() + kChevronSpacing);

  // Now we add an extra between-item padding value so the space can be divided
  // evenly by (size of icon with padding).
  return static_cast<size_t>(
      std::max(0, available_space + kItemSpacing) / IconWidth(true));
}

int BrowserActionsContainer::MinimumNonemptyWidth() const {
  if (!chevron_)
    return ToolbarView::kStandardSpacing;
  return (ToolbarView::kStandardSpacing * 2) + kChevronSpacing +
      chevron_->GetPreferredSize().width();
}

void BrowserActionsContainer::Animate(gfx::Tween::Type tween_type,
                                      size_t num_visible_icons) {
  int target_size = IconCountToWidth(num_visible_icons);
  if (resize_animation_ && !disable_animations_during_testing_) {
    // Animate! We have to set the animation_target_size_ after calling Reset(),
    // because that could end up calling AnimationEnded which clears the value.
    resize_animation_->Reset();
    resize_animation_->SetTweenType(tween_type);
    animation_target_size_ = target_size;
    resize_animation_->Show();
  } else {
    animation_target_size_ = target_size;
    AnimationEnded(resize_animation_.get());
  }
}

size_t BrowserActionsContainer::GetIconCount() const {
  if (!model_)
    return 0u;

  // Find the absolute value for the model's visible count.
  int model_visible_size = model_->GetVisibleIconCount();
  size_t absolute_model_visible_size = model_visible_size == -1 ?
      model_->toolbar_items().size() : model_visible_size;

#if !defined(NDEBUG)
  // Good time for some sanity checks: We should never try to display more
  // icons than we have, and we should always have a view per item in the model.
  // (The only exception is if this is in initialization.)
  if (initialized_) {
    size_t num_extension_actions = 0u;
    for (ToolbarActionView* view : toolbar_action_views_) {
      // No component action should ever have a valid extension id, so we can
      // use this to check the extension amount.
      // TODO(devlin): Fix this to just check model size when the model also
      // includes component actions.
      if (crx_file::id_util::IdIsValid(view->view_controller()->GetId()))
        ++num_extension_actions;
    }
    DCHECK_LE(absolute_model_visible_size, num_extension_actions);
    DCHECK_EQ(model_->toolbar_items().size(), num_extension_actions);
  }
#endif

  // The overflow displays any icons not shown by the main bar.
  return in_overflow_mode() ?
      model_->toolbar_items().size() - absolute_model_visible_size :
      absolute_model_visible_size;
}
