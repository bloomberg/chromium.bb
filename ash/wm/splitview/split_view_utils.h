// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// Animate |layer|'s opacity from 0 -> 1 if we are showing the layer, and
// 1 -> 0 if we are hiding the layer.
void AnimateSplitviewLabelOpacity(ui::Layer* layer, bool visible);

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_UTILS_H_
