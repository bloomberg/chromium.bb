// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_
#define MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_

#include "base/callback.h"

namespace mojo {

template <typename Sig>
using Callback = base::Callback<Sig>;

using Closure = base::Closure;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_CALLBACK_H_
