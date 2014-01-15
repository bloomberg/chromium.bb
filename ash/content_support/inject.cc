// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content_support/inject.h"

#include "ash/content_support/gpu_support_impl.h"
#include "ash/shell.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

void InitContentSupport() {
  scoped_ptr<GPUSupportImpl> gpu_support(new GPUSupportImpl);
  Shell::GetInstance()->SetGPUSupport(gpu_support.Pass());
}

}  // namespace ash
