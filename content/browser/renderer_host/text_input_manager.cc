// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/text_input_manager.h"

#include "base/strings/string16.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/view_messages.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace content {

namespace {

bool AreDifferentTextInputStates(const content::TextInputState& old_state,
                                 const content::TextInputState& new_state) {
#if defined(USE_AURA)
  return old_state.type != new_state.type || old_state.mode != new_state.mode ||
         old_state.flags != new_state.flags ||
         old_state.can_compose_inline != new_state.can_compose_inline;
#elif defined(OS_MACOSX)
  return old_state.type != new_state.type ||
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

  for (auto* view : views)
    Unregister(view);
}

RenderWidgetHostImpl* TextInputManager::GetActiveWidget() const {
  return !!active_view_ ? static_cast<RenderWidgetHostImpl*>(
                              active_view_->GetRenderWidgetHost())
                        : nullptr;
}

const TextInputState* TextInputManager::GetTextInputState() const {
  return !!active_view_ ? &text_input_state_map_.at(active_view_) : nullptr;
}

gfx::Rect TextInputManager::GetSelectionBoundsRect() const {
  if (!active_view_)
    return gfx::Rect();

  return gfx::RectBetweenSelectionBounds(
      selection_region_map_.at(active_view_).anchor,
      selection_region_map_.at(active_view_).focus);
}

const std::vector<gfx::Rect>* TextInputManager::GetCompositionCharacterBounds()
    const {
  return !!active_view_
             ? &composition_range_info_map_.at(active_view_).character_bounds
             : nullptr;
}

const TextInputManager::TextSelection* TextInputManager::GetTextSelection(
    RenderWidgetHostViewBase* view) const {
  DCHECK(!view || IsRegistered(view));
  if (!view)
    view = active_view_;
  return !!view ? &text_selection_map_.at(view) : nullptr;
}

void TextInputManager::UpdateTextInputState(
    RenderWidgetHostViewBase* view,
    const TextInputState& text_input_state) {
  DCHECK(IsRegistered(view));

  if (text_input_state.type == ui::TEXT_INPUT_TYPE_NONE &&
      active_view_ != view) {
    // We reached here because an IPC is received to reset the TextInputState
    // for |view|. But |view| != |active_view_|, which suggests that at least
    // one other view has become active and we have received the corresponding
    // IPC from their RenderWidget sooner than this one. That also means we have
    // already synthesized the loss of TextInputState for the |view| before (see
    // below). So we can forget about this method ever being called (no observer
    // calls necessary).
    return;
  }

  // Since |view| is registered, we already have a previous value for its
  // TextInputState.
  bool changed = AreDifferentTextInputStates(text_input_state_map_[view],
                                             text_input_state);

  text_input_state_map_[view] = text_input_state;

  // If |view| is different from |active_view| and its |TextInputState.type| is
  // not NONE, |active_view_| should change to |view|.
  if (text_input_state.type != ui::TEXT_INPUT_TYPE_NONE &&
      active_view_ != view) {
    if (active_view_) {
      // Ideally, we should always receive an IPC from |active_view_|'s
      // RenderWidget to reset its |TextInputState.type| to NONE, before any
      // other RenderWidget updates its TextInputState. But there is no
      // guarantee in the order of IPCs from different RenderWidgets and another
      // RenderWidget's IPC might arrive sooner and we reach here. To make the
      // IME behavior identical to the non-OOPIF case, we have to manually reset
      // the state for |active_view_|.
      text_input_state_map_[active_view_].type = ui::TEXT_INPUT_TYPE_NONE;
      RenderWidgetHostViewBase* active_view = active_view_;
      active_view_ = nullptr;
      NotifyObserversAboutInputStateUpdate(active_view, true);
    }
    active_view_ = view;
  }

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

void TextInputManager::ImeCompositionRangeChanged(
    RenderWidgetHostViewBase* view,
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  DCHECK(IsRegistered(view));
  composition_range_info_map_[view].character_bounds.clear();

  // The values for the bounds should be converted to root view's coordinates
  // before being stored.
  for (auto rect : character_bounds) {
    composition_range_info_map_[view].character_bounds.emplace_back(gfx::Rect(
        view->TransformPointToRootCoordSpace(rect.origin()), rect.size()));
  }

  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnImeCompositionRangeChanged(this, view));
}

void TextInputManager::SelectionChanged(RenderWidgetHostViewBase* view,
                                        const base::string16& text,
                                        size_t offset,
                                        const gfx::Range& range) {
  DCHECK(IsRegistered(view));

  text_selection_map_[view].text = text;
  text_selection_map_[view].offset = offset;
  text_selection_map_[view].range.set_start(range.start());
  text_selection_map_[view].range.set_end(range.end());

  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnTextSelectionChanged(this, view));
}

void TextInputManager::Register(RenderWidgetHostViewBase* view) {
  DCHECK(!IsRegistered(view));

  text_input_state_map_[view] = TextInputState();
  selection_region_map_[view] = SelectionRegion();
  composition_range_info_map_[view] = CompositionRangeInfo();
  text_selection_map_[view] = TextSelection();
}

void TextInputManager::Unregister(RenderWidgetHostViewBase* view) {
  DCHECK(IsRegistered(view));

  text_input_state_map_.erase(view);
  selection_region_map_.erase(view);
  composition_range_info_map_.erase(view);
  text_selection_map_.erase(view);

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

TextInputManager::CompositionRangeInfo::CompositionRangeInfo() {}

TextInputManager::CompositionRangeInfo::CompositionRangeInfo(
    const CompositionRangeInfo& other) = default;

TextInputManager::CompositionRangeInfo::~CompositionRangeInfo() {}

TextInputManager::TextSelection::TextSelection()
    : offset(0), range(gfx::Range::InvalidRange()), text(base::string16()) {}

TextInputManager::TextSelection::TextSelection(const TextSelection& other) =
    default;

TextInputManager::TextSelection::~TextSelection() {}

}  // namespace content
