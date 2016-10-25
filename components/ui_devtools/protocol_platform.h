// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_PROTOCOL_PLATFORM_H_
#define COMPONENTS_UI_DEVTOOLS_PROTOCOL_PLATFORM_H_

#include "base/memory/ptr_util.h"

namespace ui {
namespace devtools {

template <typename T>
std::unique_ptr<T> wrapUnique(T* ptr) {
  return base::WrapUnique<T>(ptr);
}

}  // namespace devtools
}  // namespace ui

#endif  // COMPONENTS_UI_DEVTOOLS_PROTOCOL_PLATFORM_H_
