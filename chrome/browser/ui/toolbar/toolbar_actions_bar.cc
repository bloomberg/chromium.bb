// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"

#include "base/auto_reset.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_delegate.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace {

#if defined(OS_MACOSX)
const int kItemSpacing = 2;
const int kLeftPadding = kItemSpacing + 1;
const int kRightPadding = 0;
const int kOverflowLeftPadding = kItemSpacing;
const int kOverflowRightPadding = kItemSpacing;
#else
// Matches ToolbarView::kStandardSpacing;
const int kLeftPadding = 3;
const int kRightPadding = kLeftPadding;
const int kItemSpacing = kLeftPadding;
const int kOverflowLeftPadding = kItemSpacing;
const int kOverflowRightPadding = kItemSpacing;
#endif

enum DimensionType { WIDTH, HEIGHT };

// Returns the width or height of the toolbar action icon size.
int GetIconDimension(DimensionType type) {
  static bool initialized = false;
  static int icon_height = 0;
  static int icon_width = 0;
  if (!initialized) {
    initialized = true;
    gfx::ImageSkia* skia =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_BROWSER_ACTION);
    icon_height = skia->height();
    icon_width = skia->width();
  }
  return type == WIDTH ? icon_width : icon_height;
}

}  // namespace

ToolbarActionsBar::PlatformSettings::PlatformSettings(bool in_overflow_mode)
    : left_padding(in_overflow_mode ? kOverflowLeftPadding : kLeftPadding),
      right_padding(in_overflow_mode ? kOverflowRightPadding : kRightPadding),
      item_spacing(kItemSpacing),
      icons_per_overflow_menu_row(1),
      chevron_enabled(!extensions::FeatureSwitch::extension_action_redesign()->
                          IsEnabled()) {
}

ToolbarActionsBar::ToolbarActionsBar(ToolbarActionsBarDelegate* delegate,
                                     Browser* browser,
                                     bool in_overflow_mode)
    : delegate_(delegate),
      browser_(browser),
      model_(extensions::ExtensionToolbarModel::Get(browser_->profile())),
      in_overflow_mode_(in_overflow_mode),
      platform_settings_(in_overflow_mode),
      model_observer_(this),
      suppress_layout_(false),
      suppress_animation_(!model_ || !model_->extensions_initialized()) {
  if (model_)  // |model_| can be null in unittests.
    model_observer_.Add(model_);
}

ToolbarActionsBar::~ToolbarActionsBar() {
  // We don't just call DeleteActions() here because it makes assumptions about
  // the order of deletion between the views and the ToolbarActionsBar.
  DCHECK(toolbar_actions_.empty()) <<
      "Must call DeleteActions() before destruction.";
}

// static
int ToolbarActionsBar::IconWidth(bool include_padding) {
  return GetIconDimension(WIDTH) + (include_padding ? kItemSpacing : 0);
}

// static
int ToolbarActionsBar::IconHeight() {
  return GetIconDimension(HEIGHT);
}

gfx::Size ToolbarActionsBar::GetPreferredSize() const {
  int icon_count = GetIconCount();
  if (in_overflow_mode_) {
    // In overflow, we always have a preferred size of a full row (even if we
    // don't use it), and always of at least one row. The parent may decide to
    // show us even when empty, e.g. as a drag target for dragging in icons from
    // the main container.
    int row_count = ((std::max(0, icon_count - 1)) /
        platform_settings_.icons_per_overflow_menu_row) + 1;
    return gfx::Size(
        IconCountToWidth(platform_settings_.icons_per_overflow_menu_row),
        row_count * IconHeight());
  }

  // If there are no actions to show (and this isn't an overflow container),
  // then don't show the container at all.
  if (toolbar_actions_.empty())
    return gfx::Size();

  return gfx::Size(IconCountToWidth(icon_count), IconHeight());
}

int ToolbarActionsBar::GetMinimumWidth() const {
  if (!platform_settings_.chevron_enabled || toolbar_actions_.empty())
    return kLeftPadding;
  return kLeftPadding + delegate_->GetChevronWidth() + kRightPadding;
}

int ToolbarActionsBar::GetMaximumWidth() const {
  return IconCountToWidth(-1);
}

int ToolbarActionsBar::IconCountToWidth(int icons) const {
  if (icons < 0)
    icons = toolbar_actions_.size();
  bool display_chevron =
      platform_settings_.chevron_enabled &&
      static_cast<size_t>(icons) < toolbar_actions_.size();
  if (icons == 0 && !display_chevron)
    return platform_settings_.left_padding;
  int icons_size = (icons == 0) ? 0 :
      (icons * IconWidth(true)) - platform_settings_.item_spacing;
  int chevron_size = display_chevron ? delegate_->GetChevronWidth() : 0;
  int padding = platform_settings_.left_padding +
                platform_settings_.right_padding;
  return icons_size + chevron_size + padding;
}

size_t ToolbarActionsBar::WidthToIconCount(int pixels) const {
  // Check for widths large enough to show the entire icon set.
  if (pixels >= IconCountToWidth(-1))
    return toolbar_actions_.size();

  // We reserve space for the padding on either side of the toolbar...
  int available_space = pixels -
      (platform_settings_.left_padding + platform_settings_.right_padding);
  // ... and, if the chevron is enabled, the chevron.
  if (platform_settings_.chevron_enabled)
    available_space -= delegate_->GetChevronWidth();

  // Now we add an extra between-item padding value so the space can be divided
  // evenly by (size of icon with padding).
  return static_cast<size_t>(std::max(
      0, available_space + platform_settings_.item_spacing) / IconWidth(true));
}

size_t ToolbarActionsBar::GetIconCount() const {
  if (!model_)
    return 0u;

  // Find the absolute value for the model's visible count.
  size_t model_visible_size = model_->GetVisibleIconCountForTab(
      browser_->tab_strip_model()->GetActiveWebContents());

#if DCHECK_IS_ON
  // Good time for some sanity checks: We should never try to display more
  // icons than we have, and we should always have a view per item in the model.
  // (The only exception is if this is in initialization.)
  if (!toolbar_actions_.empty() && !suppress_layout_ &&
      model_->extensions_initialized()) {
    size_t num_extension_actions = 0u;
    for (ToolbarActionViewController* action : toolbar_actions_) {
      // No component action should ever have a valid extension id, so we can
      // use this to check the extension amount.
      // TODO(devlin): Fix this to just check model size when the model also
      // includes component actions.
      if (crx_file::id_util::IdIsValid(action->GetId()))
        ++num_extension_actions;
    }
    DCHECK_LE(model_visible_size, num_extension_actions);
    DCHECK_EQ(model_->toolbar_items().size(), num_extension_actions);
  }
#endif

  // The overflow displays any icons not shown by the main bar.
  return in_overflow_mode_ ?
      model_->toolbar_items().size() - model_visible_size : model_visible_size;
}

void ToolbarActionsBar::CreateActions() {
  DCHECK(toolbar_actions_.empty());
  // We wait for the extension system to be initialized before we add any
  // actions, as they rely on the extension system to function.
  if (!model_ || !model_->extensions_initialized())
    return;

  {
    // We don't redraw the view while creating actions.
    base::AutoReset<bool> layout_resetter(&suppress_layout_, true);

    // Extension actions come first.
    extensions::ExtensionActionManager* action_manager =
        extensions::ExtensionActionManager::Get(browser_->profile());
    const extensions::ExtensionList& toolbar_items = model_->GetItemOrderForTab(
        GetCurrentWebContents());
    for (const scoped_refptr<const extensions::Extension>& extension :
             toolbar_items) {
      toolbar_actions_.push_back(new ExtensionActionViewController(
          extension.get(),
          browser_,
          action_manager->GetExtensionAction(*extension)));
    }

    // Component actions come second, and are suppressed if the extension
    // actions are being highlighted.
    if (!model_->is_highlighting()) {
      ScopedVector<ToolbarActionViewController> component_actions =
          ComponentToolbarActionsFactory::GetInstance()->
              GetComponentToolbarActions();
      DCHECK(component_actions.empty() ||
          extensions::FeatureSwitch::extension_action_redesign()->IsEnabled());
      toolbar_actions_.insert(toolbar_actions_.end(),
                              component_actions.begin(),
                              component_actions.end());
      component_actions.weak_clear();
    }

    for (size_t i = 0; i < toolbar_actions_.size(); ++i)
      delegate_->AddViewForAction(toolbar_actions_[i], i);
  }
}

void ToolbarActionsBar::DeleteActions() {
  delegate_->RemoveAllViews();
  toolbar_actions_.clear();
}

void ToolbarActionsBar::Update() {
  if (toolbar_actions_.empty())
    return;  // Nothing to do.

  // When we do a bulk-refresh (such as when we switch tabs), we don't
  // animate the difference. We only animate when it's a change driven by the
  // action.
  base::AutoReset<bool> animation_resetter(&suppress_animation_, true);

  {
    // Don't layout until the end.
    base::AutoReset<bool> layout_resetter(&suppress_layout_, true);
    for (ToolbarActionViewController* action : toolbar_actions_)
      action->UpdateState();
  }

  ReorderActions();  // Also triggers a draw.
}

void ToolbarActionsBar::SetOverflowRowWidth(int width) {
  DCHECK(in_overflow_mode_);
  platform_settings_.icons_per_overflow_menu_row =
      (width - kItemSpacing) / IconWidth(true);
}

void ToolbarActionsBar::OnResizeComplete(int width) {
  // Save off the desired number of visible icons.  We do this now instead of at
  // the end of the animation so that even if the browser is shut down while
  // animating, the right value will be restored on next run.
  model_->SetVisibleIconCount(WidthToIconCount(width));
}

void ToolbarActionsBar::OnDragDrop(int dragged_index,
                                   int dropped_index,
                                   DragType drag_type) {
  model_->MoveExtensionIcon(toolbar_actions_[dragged_index]->GetId(),
                            dropped_index);

  if (drag_type != DRAG_TO_SAME) {
    int delta = drag_type == DRAG_TO_OVERFLOW ? -1 : 1;
    model_->SetVisibleIconCount(model_->visible_icon_count() + delta);
  }
}

void ToolbarActionsBar::ToolbarExtensionAdded(
    const extensions::Extension* extension,
    int index) {
  DCHECK(GetActionForId(extension->id()) == nullptr) <<
      "Asked to add a toolbar action view for an extension that already exists";

  toolbar_actions_.insert(
      toolbar_actions_.begin() + index,
      new ExtensionActionViewController(
          extension,
          browser_,
          extensions::ExtensionActionManager::Get(browser_->profile())->
              GetExtensionAction(*extension)));

  delegate_->AddViewForAction(toolbar_actions_[index], index);

  // If we are still initializing the container, don't bother animating.
  if (!model_->extensions_initialized())
    return;

  // If this is just an upgrade, then don't worry about resizing.
  if (!extensions::ExtensionSystem::Get(browser_->profile())->runtime_data()->
          IsBeingUpgraded(extension)) {
    // We may need to resize (e.g. to show the new icon, or the chevron).
    int preferred_width = GetPreferredSize().width();
    if (preferred_width != delegate_->GetWidth()) {
      delegate_->ResizeAndAnimate(gfx::Tween::LINEAR,
                                  preferred_width,
                                  true);  // suppress chevron
      return;
    }
  }

  delegate_->Redraw(false);
}

void ToolbarActionsBar::ToolbarExtensionRemoved(
    const extensions::Extension* extension) {
  ToolbarActions::iterator iter = toolbar_actions_.begin();
  while (iter != toolbar_actions_.end() && (*iter)->GetId() != extension->id())
    ++iter;

  if (iter == toolbar_actions_.end())
    return;

  delegate_->RemoveViewForAction(*iter);
  toolbar_actions_.erase(iter);

  // If the extension is being upgraded we don't want the bar to shrink
  // because the icon is just going to get re-added to the same location.
  if (!extensions::ExtensionSystem::Get(browser_->profile())->runtime_data()->
            IsBeingUpgraded(extension)) {
    if (toolbar_actions_.size() > model_->visible_icon_count()) {
      // If we have more icons than we can show, then we must not be changing
      // the container size (since we either removed an icon from the main
      // area and one from the overflow list will have shifted in, or we
      // removed an entry directly from the overflow list).
      delegate_->Redraw(false);
    } else {
      delegate_->SetChevronVisibility(false);
      // Either we went from overflow to no-overflow, or we shrunk the no-
      // overflow container by 1.  Either way the size changed, so animate.
      delegate_->ResizeAndAnimate(gfx::Tween::EASE_OUT,
                                  GetPreferredSize().width(),
                                  false);  // don't suppress chevron
    }
  }
}

void ToolbarActionsBar::ToolbarExtensionMoved(
    const extensions::Extension* extension,
    int index) {
  DCHECK(index >= 0 && index < static_cast<int>(toolbar_actions_.size()));
  ToolbarActions::iterator iter = toolbar_actions_.begin();
  while (iter != toolbar_actions_.end() && (*iter)->GetId() != extension->id())
    ++iter;

  DCHECK(iter != toolbar_actions_.end());
  if (iter - toolbar_actions_.begin() == index)
    return;  // Already in place.

  ToolbarActionViewController* moved_action = *iter;
  toolbar_actions_.weak_erase(iter);
  toolbar_actions_.insert(toolbar_actions_.begin() + index, moved_action);

  delegate_->Redraw(true);
}

void ToolbarActionsBar::ToolbarExtensionUpdated(
    const extensions::Extension* extension) {
  ToolbarActionViewController* action = GetActionForId(extension->id());
  // There might not be a view in cases where we are highlighting or if we
  // haven't fully initialized the actions.
  if (action)
    action->UpdateState();
}

bool ToolbarActionsBar::ShowExtensionActionPopup(
    const extensions::Extension* extension,
    bool grant_active_tab) {
  // Don't override another popup, and only show in the active window.
  if (delegate_->IsPopupRunning() || !browser_->window()->IsActive())
    return false;

  ToolbarActionViewController* action = GetActionForId(extension->id());
  return action && action->ExecuteAction(grant_active_tab);
}

void ToolbarActionsBar::ToolbarVisibleCountChanged() {
  int desired_width = GetPreferredSize().width();
  if (desired_width != delegate_->GetWidth()) {
    delegate_->ResizeAndAnimate(gfx::Tween::EASE_OUT,
                                desired_width,
                                false);  // don't suppress chevron
  } else if (delegate_->IsAnimating()) {
    // It's possible that we're right where we're supposed to be in terms of
    // width, but that we're also currently resizing. If this is the case, end
    // the current animation with the current width.
    delegate_->StopAnimating();
  } else if (in_overflow_mode_) {
    // In overflow mode, our desired width may not change, even if we need to
    // redraw (since we have a fixed width).
    delegate_->Redraw(false);
  }
}

void ToolbarActionsBar::ToolbarHighlightModeChanged(bool is_highlighting) {
  // It's a bit of a pain that we delete and recreate everything here, but given
  // everything else going on (the lack of highlight, [n] more extensions
  // appearing, etc), it's not worth the extra complexity to create and insert
  // only the new actions.
  DeleteActions();
  CreateActions();
  delegate_->ResizeAndAnimate(gfx::Tween::LINEAR,
                              GetPreferredSize().width(),
                              true);  // suppress chevron
}

void ToolbarActionsBar::OnToolbarModelInitialized() {
  // We shouldn't have any actions before the model is initialized.
  DCHECK(toolbar_actions_.empty());
  suppress_animation_ = false;
  CreateActions();
  if (!toolbar_actions_.empty()) {
    delegate_->Redraw(false);
    ToolbarVisibleCountChanged();
  }
}

void ToolbarActionsBar::OnToolbarReorderNecessary(
    content::WebContents* web_contents) {
  if (GetCurrentWebContents() == web_contents)
    ReorderActions();
}

Browser* ToolbarActionsBar::GetBrowser() {
  return browser_;
}

void ToolbarActionsBar::ReorderActions() {
  extensions::ExtensionList new_order =
      model_->GetItemOrderForTab(GetCurrentWebContents());
  if (new_order.empty())
    return;  // Nothing to do.

#if DCHECK_IS_ON
  // Make sure the lists are in sync. There should be a view for each action in
  // the new order.
  // |toolbar_actions_| may have more views than actions are present in
  // |new_order| if there are any component toolbar actions.
  // TODO(devlin): Change this to DCHECK_EQ when all toolbar actions are shown
  // in the model.
  DCHECK_LE(new_order.size(), toolbar_actions_.size());
  for (const scoped_refptr<const extensions::Extension>& extension : new_order)
    DCHECK(GetActionForId(extension->id()));
#endif

  // Run through the views and compare them to the desired order. If something
  // is out of place, find the correct spot for it.
  for (size_t i = 0; i < new_order.size() - 1; ++i) {
    if (new_order[i]->id() != toolbar_actions_[i]->GetId()) {
      // Find where the correct view is (it's guaranteed to be after our current
      // index, since everything up to this point is correct).
      size_t j = i + 1;
      while (new_order[i]->id() != toolbar_actions_[j]->GetId())
        ++j;
      std::swap(toolbar_actions_[i], toolbar_actions_[j]);
    }
  }

  // Our visible browser actions may have changed - re-Layout() and check the
  // size.
  ToolbarVisibleCountChanged();
  delegate_->Redraw(true);
}

ToolbarActionViewController* ToolbarActionsBar::GetActionForId(
    const std::string& id) {
  for (ToolbarActionViewController* action : toolbar_actions_) {
    if (action->GetId() == id)
      return action;
  }
  return nullptr;
}

content::WebContents* ToolbarActionsBar::GetCurrentWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
