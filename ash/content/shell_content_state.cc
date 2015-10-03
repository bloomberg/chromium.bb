// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content/shell_content_state.h"

#include "base/logging.h"

namespace ash {

// static
ShellContentState* ShellContentState::instance_ = nullptr;

// static
void ShellContentState::SetInstance(ShellContentState* instance) {
  DCHECK(!instance_);
  instance_ = instance;
}

// static
ShellContentState* ShellContentState::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

// static
void ShellContentState::DestroyInstance() {
  DCHECK(instance_);
  delete instance_;
  instance_ = nullptr;
}

content::BrowserContext* ShellContentState::GetActiveBrowserContext() {
  return nullptr;
}

ShellContentState::ShellContentState() {}
ShellContentState::~ShellContentState() {}

}  // namespace ash
