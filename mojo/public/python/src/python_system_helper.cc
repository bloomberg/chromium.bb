// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/python/src/python_system_helper.h"

#include "Python.h"

#include "mojo/public/cpp/environment/logging.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"

namespace {

class ScopedGIL {
 public:
  ScopedGIL() {
    state_ = PyGILState_Ensure();
  }

  ~ScopedGIL() {
    PyGILState_Release(state_);
  }

 private:
  PyGILState_STATE state_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScopedGIL);
};

class PythonClosure : public mojo::Closure::Runnable {
 public:
  PythonClosure(PyObject* callable) : callable_(callable) {
    MOJO_CHECK(callable);
    Py_XINCREF(callable);
  }

  virtual ~PythonClosure() {
    ScopedGIL acquire_gil;
    Py_DECREF(callable_);
  }

  virtual void Run() const MOJO_OVERRIDE {
    ScopedGIL acquire_gil;
    PyObject* empty_tuple = PyTuple_New(0);
    if (!empty_tuple) {
      mojo::RunLoop::current()->Quit();
      return;
    }

    PyObject* result = PyObject_CallObject(callable_, empty_tuple);
    Py_DECREF(empty_tuple);
    if (result) {
      Py_DECREF(result);
    } else {
      mojo::RunLoop::current()->Quit();
      return;
    }
  }

 private:
  PyObject* callable_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(PythonClosure);
};

}  // namespace

namespace mojo {

Closure BuildClosure(PyObject* callable) {
  if (!PyCallable_Check(callable))
    return Closure();

  return Closure(
      static_cast<mojo::Closure::Runnable*>(new PythonClosure(callable)));
}

}  // namespace mojo
