// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGHLIGHTER_HIGHLIGHTER_CONTROLLER_TEST_API_H_
#define ASH_HIGHLIGHTER_HIGHLIGHTER_CONTROLLER_TEST_API_H_

#include <memory>

#include "base/macros.h"

namespace ash {

class FastInkPoints;
class HighlighterController;
class HighlighterSelectionObserver;

// An api for testing the HighlighterController class.
class HighlighterControllerTestApi {
 public:
  explicit HighlighterControllerTestApi(HighlighterController* instance);
  ~HighlighterControllerTestApi();

  void SetEnabled(bool enabled);
  bool IsShowingHighlighter() const;
  bool IsFadingAway() const;
  bool IsShowingSelectionResult() const;
  const FastInkPoints& points() const;
  const FastInkPoints& predicted_points() const;

 private:
  HighlighterController* instance_;
  std::unique_ptr<HighlighterSelectionObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(HighlighterControllerTestApi);
};

}  // namespace ash

#endif  // ASH_HIGHLIGHTER_HIGHLIGHTER_CONTROLLER_TEST_API_H_
