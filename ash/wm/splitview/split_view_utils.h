// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_

#include "ui/gfx/transform.h"

namespace ui {
class ImplicitAnimationObserver;
class Layer;
}  // namespace ui

namespace ash {

// Enum of the different splitview mode animations. Sorted by type (fade/slide)
// then alphabetically.
enum SplitviewAnimationType {
  // Used to fade in and out the highlights on either side which indicate where
  // to drag a selector item.
  SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_IN,
  SPLITVIEW_ANIMATION_HIGHLIGHT_FADE_OUT,
  // Used to fade in and out the other highlight. There are normally two
  // highlights, one on each side. When entering a state with a phantom
  // highlight, one highlight is the phantom highlight, and the other highlight
  // is the other highlight.
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_IN,
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_FADE_OUT,
  // Used to fade in and out the phantom highlight which indicate the bounds of
  // the window that is about to get snapped.
  SPLITVIEW_ANIMATION_PHANTOM_FADE_IN,
  SPLITVIEW_ANIMATION_PHANTOM_FADE_OUT,
  // Used to fade in and out the label on the selector item which warns users
  // the item cannot be snapped. The label appears on the selector item after
  // another window has been snapped.
  SPLITVIEW_ANIMATION_SELECTOR_ITEM_FADE_IN,
  SPLITVIEW_ANIMATION_SELECTOR_ITEM_FADE_OUT,
  // Used to fade in and out the labels which appear on either side of overview
  // mode when a selector item is selected. They indicate where to drag the
  // selector item if it is snappable, or if an item cannot be snapped.
  SPLITVIEW_ANIMATION_TEXT_FADE_IN,
  SPLITVIEW_ANIMATION_TEXT_FADE_OUT,
  // Used to slide in and out the phantom highlight.
  SPLITVIEW_ANIMATION_PHANTOM_SLIDE_IN_OUT,
  // Used to slide in and out the other highlight.
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_IN,
  SPLITVIEW_ANIMATION_OTHER_HIGHLIGHT_SLIDE_OUT,
};

// Animates |layer|'s opacity based on |type|.
void DoSplitviewOpacityAnimation(ui::Layer* layer, SplitviewAnimationType type);

// Animates |layer|'s transform based on |type|. |observer| can be passed if the
// caller wants to do an action after the animation has ended.
void DoSplitviewTransformAnimation(ui::Layer* layer,
                                   SplitviewAnimationType type,
                                   const gfx::Transform& target_transform,
                                   ui::ImplicitAnimationObserver* observer);

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_
