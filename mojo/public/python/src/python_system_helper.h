// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_PYTHON_SRC_PYTHON_SYSTEM_HELPER_H_
#define MOJO_PUBLIC_PYTHON_SRC_PYTHON_SYSTEM_HELPER_H_

// Python must be the first include, as it defines preprocessor variable without
// checking if they already exist.
#include <Python.h>

#include <map>

#include "mojo/public/c/environment/async_waiter.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/bindings/lib/shared_ptr.h"

namespace mojo {
namespace python {

// Create a mojo::Closure from a callable python object.
mojo::Closure BuildClosure(PyObject* callable);

class PythonAsyncWaiter {
 public:
  PythonAsyncWaiter();
  ~PythonAsyncWaiter();
  MojoAsyncWaitID AsyncWait(MojoHandle handle,
                            MojoHandleSignals signals,
                            MojoDeadline deadline,
                            PyObject* callable);

  void CancelWait(MojoAsyncWaitID wait_id);

 private:
  class AsyncWaiterRunnable;

  typedef std::map<MojoAsyncWaitID,
                   internal::SharedPtr<mojo::Callback<void(MojoResult)> > >
      CallbackMap;

  CallbackMap callbacks_;
  const MojoAsyncWaiter* async_waiter_;
};

}  // namespace python
}  // namespace mojo

#endif  // MOJO_PUBLIC_PYTHON_SRC_PYTHON_SYSTEM_HELPER_H_
