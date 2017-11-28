// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mus_util.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace content {

bool IsUsingMus() {
#if defined(USE_AURA)
  return aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS;
#else
  return false;
#endif
}

}  // namespace content
