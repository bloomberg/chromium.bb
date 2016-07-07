// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_manager.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/view_messages.h"

namespace content {

namespace {

bool AreDifferentTextInputStates(const content::TextInputState& old_state,
                                 const content::TextInputState& new_state) {
#if defined(USE_AURA)
  return old_state.type != new_state.type || old_state.mode != new_state.mode ||
         old_state.flags != new_state.flags ||
         old_state.can_compose_inline != new_state.can_compose_inline;
#else
  // TODO(ekaramad): Implement the logic for other platforms (crbug.com/578168).
  NOTREACHED();
  return true;
#endif
}

}  // namespace

TextInputManager::TextInputManager() : active_view_(nullptr) {}

TextInputManager::~TextInputManager() {
  // If there is an active view, we should unregister it first so that the
  // the tab's top-level RWHV will be notified about |TextInputState.type|
  // resetting to none (i.e., we do not have an active RWHV anymore).
  if (active_view_)
    Unregister(active_view_);

  // Unregister all the remaining views.
  std::vector<RenderWidgetHostViewBase*> views;
  for (auto pair : text_input_state_map_)
    views.push_back(pair.first);

  for (auto view : views)
    Unregister(view);
}

const TextInputState* TextInputManager::GetTextInputState() {
  return !!active_view_ ? &text_input_state_map_[active_view_] : nullptr;
}

RenderWidgetHostImpl* TextInputManager::GetActiveWidget() const {
  return !!active_view_ ? static_cast<RenderWidgetHostImpl*>(
                              active_view_->GetRenderWidgetHost())
                        : nullptr;
}

gfx::Rect TextInputManager::GetSelectionBoundsRect() {
  if (!active_view_)
    return gfx::Rect();

  return gfx::RectBetweenSelectionBounds(
      selection_region_map_[active_view_].anchor,
      selection_region_map_[active_view_].focus);
}

void TextInputManager::UpdateTextInputState(
    RenderWidgetHostViewBase* view,
    const TextInputState& text_input_state) {
  DCHECK(IsRegistered(view));

  // Since |view| is registgered, we already have a previous value for its
  // TextInputState.
  bool changed = AreDifferentTextInputStates(text_input_state_map_[view],
                                             text_input_state);

  text_input_state_map_[view] = text_input_state;

  // |active_view_| is only updated when the state for |view| is not none.
  if (text_input_state.type != ui::TEXT_INPUT_TYPE_NONE)
    active_view_ = view;

  // If the state for |active_view_| is none, then we no longer have an
  // |active_view_|.
  if (active_view_ == view && text_input_state.type == ui::TEXT_INPUT_TYPE_NONE)
    active_view_ = nullptr;

  NotifyObserversAboutInputStateUpdate(view, changed);
}

void TextInputManager::ImeCancelComposition(RenderWidgetHostViewBase* view) {
  DCHECK(IsRegistered(view));
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnImeCancelComposition(this, view));
}

void TextInputManager::SelectionBoundsChanged(
    RenderWidgetHostViewBase* view,
    const ViewHostMsg_SelectionBounds_Params& params) {
  DCHECK(IsRegistered(view));

// TODO(ekaramad): Implement the logic for other platforms (crbug.com/578168).
#if defined(USE_AURA)
  gfx::SelectionBound anchor_bound, focus_bound;
  // Converting the points to the |view|'s root coordinate space (for child
  // frame views).
  anchor_bound.SetEdge(gfx::PointF(view->TransformPointToRootCoordSpace(
                           params.anchor_rect.origin())),
                       gfx::PointF(view->TransformPointToRootCoordSpace(
                           params.anchor_rect.bottom_left())));
  focus_bound.SetEdge(gfx::PointF(view->TransformPointToRootCoordSpace(
                          params.focus_rect.origin())),
                      gfx::PointF(view->TransformPointToRootCoordSpace(
                          params.focus_rect.bottom_left())));

  if (params.anchor_rect == params.focus_rect) {
    anchor_bound.set_type(gfx::SelectionBound::CENTER);
    focus_bound.set_type(gfx::SelectionBound::CENTER);
  } else {
    // Whether text is LTR at the anchor handle.
    bool anchor_LTR = params.anchor_dir == blink::WebTextDirectionLeftToRight;
    // Whether text is LTR at the focus handle.
    bool focus_LTR = params.focus_dir == blink::WebTextDirectionLeftToRight;

    if ((params.is_anchor_first && anchor_LTR) ||
        (!params.is_anchor_first && !anchor_LTR)) {
      anchor_bound.set_type(gfx::SelectionBound::LEFT);
    } else {
      anchor_bound.set_type(gfx::SelectionBound::RIGHT);
    }
    if ((params.is_anchor_first && focus_LTR) ||
        (!params.is_anchor_first && !focus_LTR)) {
      focus_bound.set_type(gfx::SelectionBound::RIGHT);
    } else {
      focus_bound.set_type(gfx::SelectionBound::LEFT);
    }
  }

  if (anchor_bound == selection_region_map_[view].anchor &&
      focus_bound == selection_region_map_[view].focus)
    return;

  selection_region_map_[view].anchor = anchor_bound;
  selection_region_map_[view].focus = focus_bound;

  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnSelectionBoundsChanged(this, view));
#endif
}

void TextInputManager::Register(RenderWidgetHostViewBase* view) {
  DCHECK(!IsRegistered(view));

  text_input_state_map_[view] = TextInputState();
}

void TextInputManager::Unregister(RenderWidgetHostViewBase* view) {
  DCHECK(IsRegistered(view));

  text_input_state_map_.erase(view);
  if (active_view_ == view) {
    active_view_ = nullptr;
    NotifyObserversAboutInputStateUpdate(view, true);
  }
  view->DidUnregisterFromTextInputManager(this);
}

bool TextInputManager::IsRegistered(RenderWidgetHostViewBase* view) const {
  return text_input_state_map_.count(view) == 1;
}

void TextInputManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void TextInputManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

size_t TextInputManager::GetRegisteredViewsCountForTesting() {
  return text_input_state_map_.size();
}

ui::TextInputType TextInputManager::GetTextInputTypeForViewForTesting(
    RenderWidgetHostViewBase* view) {
  DCHECK(IsRegistered(view));
  return text_input_state_map_[view].type;
}

void TextInputManager::NotifyObserversAboutInputStateUpdate(
    RenderWidgetHostViewBase* updated_view,
    bool did_update_state) {
  FOR_EACH_OBSERVER(
      Observer, observer_list_,
      OnUpdateTextInputStateCalled(this, updated_view, did_update_state));
}

TextInputManager::SelectionRegion::SelectionRegion() {}

TextInputManager::SelectionRegion::SelectionRegion(
    const SelectionRegion& other) = default;

}  // namespace content