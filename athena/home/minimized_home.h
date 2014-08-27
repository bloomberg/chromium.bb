// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_MINIMIZED_HOME_H_
#define ATHENA_HOME_MINIMIZED_HOME_H_

#include "base/memory/scoped_ptr.h"

namespace ui {
class LayerOwner;
}

namespace athena {

scoped_ptr<ui::LayerOwner> CreateMinimizedHome();

}  // namespace athena

#endif  // ATHENA_HOME_MINIMIZED_HOME_H_
