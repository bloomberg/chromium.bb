// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gamepad/gamepad_data_fetcher.h"

namespace content {

GamepadDataFetcher::GamepadDataFetcher() : provider_(nullptr) {
}

void GamepadDataFetcher::InitializeProvider(GamepadProvider* provider) {
  DCHECK(provider);

  provider_ = provider;
  OnAddedToProvider();
}

}  // namespace content
