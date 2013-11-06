// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/bindings_support.h"

#include <assert.h>
#include <stddef.h>

namespace mojo {

namespace {
BindingsSupport* g_bindings_support = NULL;
}

// static
void BindingsSupport::Set(BindingsSupport* support) {
  g_bindings_support = support;
}

// static
BindingsSupport* BindingsSupport::Get() {
  assert(g_bindings_support);
  return g_bindings_support;
}

}  // namespace mojo
