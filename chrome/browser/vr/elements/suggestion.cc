// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/suggestion.h"

#include "base/bind.h"

namespace vr {

Suggestion::Suggestion(base::Callback<void(GURL)> navigate_callback)
    : LinearLayout(LinearLayout::kRight),
      navigate_callback_(navigate_callback) {
  EventHandlers event_handlers;
  event_handlers.button_up =
      base::Bind(&Suggestion::HandleButtonClick, base::Unretained(this));
  set_event_handlers(event_handlers);
}

Suggestion::~Suggestion() = default;

void Suggestion::HandleButtonClick() {
  navigate_callback_.Run(destination_);
}

}  // namespace vr
