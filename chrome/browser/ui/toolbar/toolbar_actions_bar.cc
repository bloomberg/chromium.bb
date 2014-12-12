// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"

#include "base/auto_reset.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
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

// static
bool ToolbarActionsBar::disable_animations_for_testing_ = false;

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
      tab_strip_observer_(this),
      suppress_layout_(false),
      suppress_animation_(true) {
  if (model_)  // |model_| can be null in unittests.
    model_observer_.Add(model_);
  tab_strip_observer_.Add(browser_->tab_strip_model());
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

  size_t visible_icons = model_->visible_icon_count();
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
    DCHECK_LE(visible_icons, num_extension_actions);
    DCHECK_EQ(model_->toolbar_items().size(), num_extension_actions);
  }
#endif

  int tab_id = SessionTabHelper::IdForTab(
      browser_->tab_strip_model()->GetActiveWebContents());
  for (ToolbarActionViewController* action : toolbar_actions_) {
    auto actions_tabs = popped_out_in_tabs_.find(action);
    if (actions_tabs != popped_out_in_tabs_.end() &&
        actions_tabs->second.count(tab_id))
      ++visible_icons;
  }

  // The overflow displays any icons not shown by the main bar.
  return in_overflow_mode_ ?
      model_->toolbar_items().size() - visible_icons : visible_icons;
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
    const extensions::ExtensionList& toolbar_items = model_->toolbar_items();
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

    if (!toolbar_actions_.empty())
      ReorderActions();

    for (size_t i = 0; i < toolbar_actions_.size(); ++i)
      delegate_->AddViewForAction(toolbar_actions_[i], i);
  }

  // Once the actions are created, we should animate the changes.
  suppress_animation_ = false;
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
      std::max((width - kItemSpacing) / IconWidth(true), 1);
}

void ToolbarActionsBar::OnResizeComplete(int width) {
  DCHECK(!in_overflow_mode_);  // The user can't resize the overflow container.
  size_t resized_count = WidthToIconCount(width);
  size_t tab_icon_count = GetIconCount();
  int tab_id = SessionTabHelper::IdForTab(GetCurrentWebContents());
  int delta = tab_icon_count - model_->visible_icon_count();
  bool reorder_necessary = false;
  if (resized_count < tab_icon_count) {
    for (int i = resized_count; i < delta; ++i) {
      // If an extension that was popped out to act is overflowed, then it
      // should no longer be popped out, and it also doesn't count for adjusting
      // the visible count (since it wasn't really out to begin with).
      if (popped_out_in_tabs_[toolbar_actions_[i]].count(tab_id)) {
        reorder_necessary = true;
        popped_out_in_tabs_[toolbar_actions_[i]].erase(tab_id);
        ++resized_count;
      }
    }
  } else {
    // If the user increases the toolbar size while actions that popped out are
    // visible, we need to re-arrange the icons in other windows to be
    // consistent with what the user sees.
    // That is, if the normal order is A, B, [C, D] (with C and D hidden), C
    // pops out to act, and then the user increases the size of the toolbar,
    // the user sees uncovering D (since C is already out). This is what should
    // happen in all windows.
    for (size_t i = tab_icon_count; i < resized_count; ++i) {
      if (toolbar_actions_[i]->GetId() !=
              model_->toolbar_items()[i - delta]->id())
        model_->MoveExtensionIcon(toolbar_actions_[i]->GetId(), i - delta);
    }
  }
  resized_count -= delta;
  // Save off the desired number of visible icons.  We do this now instead of at
  // the end of the animation so that even if the browser is shut down while
  // animating, the right value will be restored on next run.
  model_->SetVisibleIconCount(resized_count);
  if (reorder_necessary)
    ReorderActions();
}

void ToolbarActionsBar::OnDragDrop(int dragged_index,
                                   int dropped_index,
                                   DragType drag_type) {
  ToolbarActionViewController* action = toolbar_actions_[dragged_index];
  int tab_id = SessionTabHelper::IdForTab(GetCurrentWebContents());
  int delta = 0;
  switch (drag_type) {
    case DRAG_TO_OVERFLOW:
      // If the user moves an action back into overflow, then we don't adjust
      // the base visible count, but do stop popping that action out.
      if (popped_out_in_tabs_[action].count(tab_id))
        popped_out_in_tabs_[action].erase(tab_id);
      else
        delta = -1;
      break;
    case DRAG_TO_MAIN:
      delta = 1;
      break;
    case DRAG_TO_SAME:
      // If the user moves an action that had popped out to be on the toolbar,
      // then we treat it as "pinning" the action, and adjust the base visible
      // count to accommodate.
      if (popped_out_in_tabs_[action].count(tab_id)) {
        delta = 1;
        popped_out_in_tabs_[action].erase(tab_id);
      }
      break;
  }

  // If there are any actions that are in front of the dropped index only
  // because they were popped out, decrement the dropped index.
  for (int i = 0; i < dropped_index; ++i) {
    if (i != dragged_index &&
        model_->GetIndexForId(toolbar_actions_[i]->GetId()) >= dropped_index)
      --dropped_index;
  }

  model_->MoveExtensionIcon(action->GetId(), dropped_index);

  if (delta)
    model_->SetVisibleIconCount(model_->visible_icon_count() + delta);
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
  if (extensions::ExtensionSystem::Get(browser_->profile())->runtime_data()->
          IsBeingUpgraded(extension->id())) {
    delegate_->Redraw(false);
  } else {
    // We may need to resize (e.g. to show the new icon, or the chevron).
    // We suppress the chevron during animation because, if we're expanding to
    // show a new icon, we don't want to have the chevron visible only for the
    // duration of the animation.
    ResizeDelegate(gfx::Tween::LINEAR, true);
  }
}

void ToolbarActionsBar::ToolbarExtensionRemoved(
    const extensions::Extension* extension) {
  ToolbarActions::iterator iter = toolbar_actions_.begin();
  while (iter != toolbar_actions_.end() && (*iter)->GetId() != extension->id())
    ++iter;

  if (iter == toolbar_actions_.end())
    return;

  delegate_->RemoveViewForAction(*iter);
  popped_out_in_tabs_.erase(*iter);
  toolbar_actions_.erase(iter);

  // If the extension is being upgraded we don't want the bar to shrink
  // because the icon is just going to get re-added to the same location.
  if (!extensions::ExtensionSystem::Get(browser_->profile())->runtime_data()->
            IsBeingUpgraded(extension->id())) {
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
      ResizeDelegate(gfx::Tween::EASE_OUT, false);
    }
  }
}

void ToolbarActionsBar::ToolbarExtensionMoved(
    const extensions::Extension* extension,
    int index) {
  DCHECK(index >= 0 && index < static_cast<int>(toolbar_actions_.size()));
  // Unfortunately, |index| doesn't really mean a lot to us, because this
  // window's toolbar could be different (if actions are popped out). Just
  // do a full reorder.
  ReorderActions();
}

void ToolbarActionsBar::ToolbarExtensionUpdated(
    const extensions::Extension* extension) {
  ToolbarActionViewController* action = GetActionForId(extension->id());
  // There might not be a view in cases where we are highlighting or if we
  // haven't fully initialized the actions.
  if (action) {
    content::WebContents* web_contents = GetCurrentWebContents();
    action->UpdateState();
    bool wants_to_run = action->WantsToRun(web_contents);

    // The action may need to be popped in or out of overflow.
    int index = std::find(toolbar_actions_.begin(),
                          toolbar_actions_.end(),
                          action) - toolbar_actions_.begin();
    bool reorder_necessary = false;
    int tab_id = SessionTabHelper::IdForTab(web_contents);
    if (wants_to_run && static_cast<size_t>(index) >= GetIconCount()) {
      popped_out_in_tabs_[action].insert(tab_id);
      reorder_necessary = true;
    } else if (!wants_to_run && popped_out_in_tabs_[action].count(tab_id)) {
      popped_out_in_tabs_[action].erase(tab_id);
      reorder_necessary = true;
    }
    if (reorder_necessary)
      ReorderActions();
  }
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
  ResizeDelegate(gfx::Tween::EASE_OUT, false);
}

void ToolbarActionsBar::ResizeDelegate(gfx::Tween::Type tween_type,
                                       bool suppress_chevron) {
  int desired_width = GetPreferredSize().width();
  if (desired_width != delegate_->GetWidth()) {
    delegate_->ResizeAndAnimate(tween_type, desired_width, suppress_chevron);
  } else if (delegate_->IsAnimating()) {
    // It's possible that we're right where we're supposed to be in terms of
    // width, but that we're also currently resizing. If this is the case, end
    // the current animation with the current width.
    delegate_->StopAnimating();
  } else {
    // We may already be at the right size (this can happen frequently with
    // overflow, where we have a fixed width, and in tests, where we skip
    // animations). If this is the case, we still need to Redraw(), because the
    // icons within the toolbar may have changed (e.g. if we removed one
    // action and added a different one in quick succession).
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
  // Resize the delegate. We suppress the chevron so that we don't risk showing
  // it only for the duration of the animation.
  ResizeDelegate(gfx::Tween::LINEAR, true);
}

void ToolbarActionsBar::OnToolbarModelInitialized() {
  // We shouldn't have any actions before the model is initialized.
  DCHECK(toolbar_actions_.empty());
  CreateActions();
  ResizeDelegate(gfx::Tween::EASE_OUT, false);
}

Browser* ToolbarActionsBar::GetBrowser() {
  return browser_;
}

void ToolbarActionsBar::TabInsertedAt(content::WebContents* web_contents,
                                      int index,
                                      bool foreground) {
  if (foreground)
    ReorderActions();
}

void ToolbarActionsBar::TabDetachedAt(content::WebContents* web_contents,
                                      int index) {
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  for (auto& tabs : popped_out_in_tabs_)
    tabs.second.erase(tab_id);
  tabs_checked_for_pop_out_.erase(tab_id);
}

void ToolbarActionsBar::TabStripModelDeleted() {
  tab_strip_observer_.RemoveAll();
}

void ToolbarActionsBar::ReorderActions() {
  if (toolbar_actions_.empty())
    return;

  // First, reset the order to that of the model. Run through the views and
  // compare them to the model; if something is out of place, find the correct
  // spot for it.
  const extensions::ExtensionList& model_order = model_->toolbar_items();
  for (int i = 0; i < static_cast<int>(model_order.size() - 1); ++i) {
    if (model_order[i]->id() != toolbar_actions_[i]->GetId()) {
      // Find where the correct view is (it's guaranteed to be after our current
      // index, since everything up to this point is correct).
      size_t j = i + 1;
      while (model_order[i]->id() != toolbar_actions_[j]->GetId())
        ++j;
      std::swap(toolbar_actions_[i], toolbar_actions_[j]);
    }
  }

  // Only adjust the order if the model isn't highlighting a particular subset.
  if (!model_->is_highlighting()) {
    // First, make sure that we've checked any actions that want to run.
    content::WebContents* web_contents = GetCurrentWebContents();
    int tab_id = SessionTabHelper::IdForTab(web_contents);
    if (!tabs_checked_for_pop_out_.count(tab_id)) {
      tabs_checked_for_pop_out_.insert(tab_id);
      for (ToolbarActionViewController* toolbar_action : toolbar_actions_) {
        if (toolbar_action->WantsToRun(web_contents))
          popped_out_in_tabs_[toolbar_action].insert(tab_id);
      }
    }

    // Then, shift any actions that want to run to the front.
    size_t insert_at = 0;
    // Rotate any actions that want to run to the boundary between visible and
    // overflowed actions.
    for (ToolbarActions::iterator iter =
             toolbar_actions_.begin() + model_->visible_icon_count();
         iter != toolbar_actions_.end(); ++iter) {
      if (popped_out_in_tabs_[(*iter)].count(tab_id)) {
        std::rotate(toolbar_actions_.begin() + insert_at, iter, iter + 1);
        ++insert_at;
      }
    }
  }

  // Our visible browser actions may have changed - re-Layout() and check the
  // size (if we aren't suppressing the layout).
  if (!suppress_layout_) {
    ResizeDelegate(gfx::Tween::EASE_OUT, false);
    delegate_->Redraw(true);
  }
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
