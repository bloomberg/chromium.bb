// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_MANAGER_H__
#define CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_MANAGER_H__

#include <unordered_map>
#include <utility>

#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/common/text_input_state.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/selection_bound.h"

struct ViewHostMsg_SelectionBounds_Params;

namespace content {
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class WebContents;

// A class which receives updates of TextInputState from multiple sources and
// decides what the new TextInputState is. It also notifies the observers when
// text input state is updated.
class CONTENT_EXPORT TextInputManager {
 public:
  // The tab's top-level RWHV should be an observer of TextInputManager to get
  // notifications about changes in TextInputState or other IME related state
  // for child frames.
  class CONTENT_EXPORT Observer {
   public:
    // Called when a view has called UpdateTextInputState on TextInputManager.
    // If the call has led to a change in TextInputState, |did_update_state| is
    // true. In some plaforms, we need this update even when the state has not
    // changed (e.g., Aura for updating IME).
    virtual void OnUpdateTextInputStateCalled(
        TextInputManager* text_input_manager,
        RenderWidgetHostViewBase* updated_view,
        bool did_update_state) {}
    // Called when |updated_view| has called ImeCancelComposition on
    // TextInputManager.
    virtual void OnImeCancelComposition(
        TextInputManager* text_input_manager,
        RenderWidgetHostViewBase* updated_view) {}
    // Called when the selection bounds for the |updated_view| has changed.
    virtual void OnSelectionBoundsChanged(
        TextInputManager* text_input_manager,
        RenderWidgetHostViewBase* updated_view) {}
  };

  TextInputManager();
  ~TextInputManager();

  // ---------------------------------------------------------------------------
  // The following methods can be used to obtain information about IME-related
  // state for the active RenderWidget.

  // Returns the currently active widget, i.e., the RWH which is associated with
  // |active_view_|.
  RenderWidgetHostImpl* GetActiveWidget() const;

  // Returns the TextInputState corresponding to |active_view_|.
  // Users of this method should not hold on to the pointer as it might become
  // dangling if the TextInputManager or |active_view_| might go away.
  const TextInputState* GetTextInputState();

  // Returns the rect between selection bounds for the |active_view_|.
  gfx::Rect GetSelectionBoundsRect();

  // ---------------------------------------------------------------------------
  // The following methods are called by RWHVs on the tab to update their IME-
  // related state.

  // Updates the TextInputState for |view|.
  void UpdateTextInputState(RenderWidgetHostViewBase* view,
                            const TextInputState& state);

  // The current IME composition has been cancelled on the renderer side for
  // the widget corresponding to |view|.
  void ImeCancelComposition(RenderWidgetHostViewBase* view);

  // Updates the selection bounds for the |view|. In Aura, selection bounds are
  // used to provide the InputMethod with the position of the caret, e.g., in
  // setting the position of the ui::ImeWindow.
  void SelectionBoundsChanged(RenderWidgetHostViewBase* view,
                              const ViewHostMsg_SelectionBounds_Params& params);

  // Registers the given |view| for tracking its TextInputState. This is called
  // by any view which has updates in its TextInputState (whether tab's RWHV or
  // that of a child frame). The |view| must unregister itself before being
  // destroyed (i.e., call TextInputManager::Unregister).
  void Register(RenderWidgetHostViewBase* view);

  // Clears the TextInputState from the |view|. If |view == active_view_|, this
  // call will lead to a TextInputState update since the TextInputState.type
  // should be reset to none.
  void Unregister(RenderWidgetHostViewBase* view);

  // Returns true if |view| is already registered.
  bool IsRegistered(RenderWidgetHostViewBase* view) const;

  // Add and remove observers for notifications regarding updates in the
  // TextInputState. Clients must be sure to remove themselves before they go
  // away.
  // Only the tab's RWHV should observer TextInputManager. In tests, however,
  // in addition to the tab's RWHV, one or more test observers might observe
  // TextInputManager.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  RenderWidgetHostViewBase* active_view_for_testing() { return active_view_; }
  size_t GetRegisteredViewsCountForTesting();
  ui::TextInputType GetTextInputTypeForViewForTesting(
      RenderWidgetHostViewBase* view);

 private:
  // Text selection bounds.
  struct SelectionRegion {
    SelectionRegion();
    SelectionRegion(const SelectionRegion& other);

    // The begining of the selection region.
    gfx::SelectionBound anchor;
    // The end of the selection region (caret position).
    gfx::SelectionBound focus;
  };

  // This class is used to create maps which hold specific IME state for a
  // view.
  template <class Value>
  class ViewMap : public std::unordered_map<RenderWidgetHostViewBase*, Value> {
  };

  void NotifyObserversAboutInputStateUpdate(RenderWidgetHostViewBase* view,
                                            bool did_update_state);

  // The view with active text input state, i.e., a focused <input> element.
  // It will be nullptr if no such view exists. Note that the active view
  // cannot have a |TextInputState.type| of ui::TEXT_INPUT_TYPE_NONE.
  RenderWidgetHostViewBase* active_view_;

  ViewMap<TextInputState> text_input_state_map_;

  // Text selection bounds information for registered views.
  ViewMap<SelectionRegion> selection_region_map_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(TextInputManager);
};
}

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_MANAGER_H__