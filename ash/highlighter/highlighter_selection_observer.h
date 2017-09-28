// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGHLIGHTER_HIGHLIGHTER_SELECTION_OBSERVER_H_
#define ASH_HIGHLIGHTER_HIGHLIGHTER_SELECTION_OBSERVER_H_

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

// Observer for handling highlighter selection.
class HighlighterSelectionObserver {
 public:
  virtual ~HighlighterSelectionObserver() {}

  virtual void HandleSelection(const gfx::Rect& rect) = 0;
  virtual void HandleFailedSelection() = 0;
};

}  // namespace ash

#endif  // ASH_HIGHLIGHTER_HIGHLIGHTER_SELECTION_OBSERVER_H_
