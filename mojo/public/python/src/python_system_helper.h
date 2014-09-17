// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_PYTHON_SRC_PYTHON_SYSTEM_HELPER_H_
#define MOJO_PUBLIC_PYTHON_SRC_PYTHON_SYSTEM_HELPER_H_

#include "Python.h"

#include "mojo/public/cpp/bindings/callback.h"

namespace mojo {

// Create a mojo::Closure from a callable python object.
mojo::Closure BuildClosure(PyObject* callable);

// Initalize mojo::RunLoop
void InitRunLoop();

}  // namespace mojo

#endif  // MOJO_PUBLIC_PYTHON_SRC_PYTHON_SYSTEM_HELPER_H_
