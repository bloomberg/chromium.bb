// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate/logo_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

LogoView::LogoView() = default;

LogoView::~LogoView() = default;

void LogoView::SetState(State state) {
  switch (state) {
    case State::kUndefined:
      SetImage(gfx::ImageSkia());
      break;
    case State::kListening:
      SetImage(gfx::CreateVectorIcon(kDotsIcon, GetPreferredSize().height()));
      break;
    case State::kMic:
      SetImage(gfx::CreateVectorIcon(kMicIcon, GetPreferredSize().height()));
      break;
  }
}

}  // namespace ash
