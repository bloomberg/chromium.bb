// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/native_handler.h"

namespace extensions {

NativeHandler::NativeHandler() : is_valid_(true) {}

NativeHandler::~NativeHandler() {}

void NativeHandler::Invalidate() { is_valid_ = false; }

}  // namespace extensions
