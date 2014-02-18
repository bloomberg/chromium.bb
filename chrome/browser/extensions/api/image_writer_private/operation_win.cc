// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"

namespace extensions {
namespace image_writer {

void Operation::WriteStart() {
  Error(error::kUnsupportedOperation);
}

void Operation::VerifyWriteStart() {
  Error(error::kUnsupportedOperation);
}

} // namespace image_writer
} // namespace extensions
