// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/environment/environment.h"

namespace mojo {

// These methods do nothing as we rely on LazyInstance<T> to instantiate all of
// our global state in this implementation of the environment library.

Environment::Environment() {
  // no-op
}

Environment::~Environment() {
  // no-op
}

}  // namespace mojo
