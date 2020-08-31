// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_DEFAULT_CONTEXT_H_
#define BASE_FUCHSIA_DEFAULT_CONTEXT_H_

#include <memory>

#include "base/base_export.h"

namespace sys {
class ComponentContext;
}  // namespace sys

namespace base {

namespace fuchsia {
// Returns default sys::ComponentContext for the current process.
BASE_EXPORT sys::ComponentContext* ComponentContextForCurrentProcess();
}  // namespace fuchsia

// Replaces the default sys::ComponentContext for the current process, and
// returns the previously-active one.
// Use the base::TestComponentContextForProcess rather than calling this
// directly.
BASE_EXPORT std::unique_ptr<sys::ComponentContext>
ReplaceComponentContextForCurrentProcessForTest(
    std::unique_ptr<sys::ComponentContext> context);

}  // namespace base

#endif  // BASE_FUCHSIA_DEFAULT_CONTEXT_H_
