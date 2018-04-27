// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_ELF_READER_LINUX_H_
#define BASE_DEBUG_ELF_READER_LINUX_H_

#include <string>

#include "base/base_export.h"
#include "base/optional.h"

namespace base {
namespace debug {

// Returns the ELF section .note.gnu.build-id from current executable file, if
// present.
Optional<std::string> BASE_EXPORT ReadElfBuildId();

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_ELF_READER_LINUX_H_
