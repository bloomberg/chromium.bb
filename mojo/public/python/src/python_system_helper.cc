// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/python/src/python_system_helper.h"

#include "Python.h"

#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/environment/logging.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"

namespace {

class ScopedGIL {
 public:
  ScopedGIL() { state_ = PyGILState_Ensure(); }

  ~ScopedGIL() { PyGILState_Release(state_); }

 private:
  PyGILState_STATE state_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScopedGIL);
};

class PythonClosure : public mojo::Closure::Runnable {
 public:
  PythonClosure(PyObject* callable) : callable_(callable) {
    MOJO_DCHECK(callable);
    Py_XINCREF(callable);
  }

  virtual ~PythonClosure() {
    ScopedGIL acquire_gil;
    Py_DECREF(callable_);
  }

  virtual void Run() const override {
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

void AsyncCallbackForwarder(void* closure, MojoResult result) {
  mojo::Callback<void(MojoResult)>* callback =
      static_cast<mojo::Callback<void(MojoResult)>*>(closure);
  // callback will be deleted when it is run.
  callback->Run(result);
}

}  // namespace

namespace mojo {
namespace python {

class PythonAsyncWaiter::AsyncWaiterRunnable
    : public mojo::Callback<void(MojoResult)>::Runnable {
 public:
  AsyncWaiterRunnable(PyObject* callable, CallbackMap* callbacks)
      : wait_id_(0), callable_(callable), callbacks_(callbacks) {
    MOJO_DCHECK(callable);
    MOJO_DCHECK(callbacks_);
    Py_XINCREF(callable);
  }

  virtual ~AsyncWaiterRunnable() {
    ScopedGIL acquire_gil;
    Py_DECREF(callable_);
  }

  void set_wait_id(int wait_id) { wait_id_ = wait_id; }

  virtual void Run(MojoResult mojo_result) const override {
    MOJO_DCHECK(wait_id_);

    // Remove to reference to this object from PythonAsyncWaiter and ensure this
    // object will be destroyed when this method exits.
    MOJO_DCHECK(callbacks_->find(wait_id_) != callbacks_->end());
    internal::SharedPtr<mojo::Callback<void(MojoResult)> > self =
        (*callbacks_)[wait_id_];
    callbacks_->erase(wait_id_);

    ScopedGIL acquire_gil;
    PyObject* args_tuple = Py_BuildValue("(i)", mojo_result);
    if (!args_tuple) {
      mojo::RunLoop::current()->Quit();
      return;
    }

    PyObject* result = PyObject_CallObject(callable_, args_tuple);
    Py_DECREF(args_tuple);
    if (result) {
      Py_DECREF(result);
    } else {
      mojo::RunLoop::current()->Quit();
      return;
    }
  }

 private:
  MojoAsyncWaitID wait_id_;
  PyObject* callable_;
  CallbackMap* callbacks_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(AsyncWaiterRunnable);
};

Closure BuildClosure(PyObject* callable) {
  if (!PyCallable_Check(callable))
    return Closure();

  return Closure(
      static_cast<mojo::Closure::Runnable*>(new PythonClosure(callable)));
}

PythonAsyncWaiter::PythonAsyncWaiter() {
  async_waiter_ = Environment::GetDefaultAsyncWaiter();
}

PythonAsyncWaiter::~PythonAsyncWaiter() {
  for (CallbackMap::const_iterator it = callbacks_.begin();
       it != callbacks_.end();
       ++it) {
    async_waiter_->CancelWait(it->first);
  }
}

MojoAsyncWaitID PythonAsyncWaiter::AsyncWait(MojoHandle handle,
                                             MojoHandleSignals signals,
                                             MojoDeadline deadline,
                                             PyObject* callable) {
  AsyncWaiterRunnable* runner = new AsyncWaiterRunnable(callable, &callbacks_);
  internal::SharedPtr<mojo::Callback<void(MojoResult)> > callback(
      new mojo::Callback<void(MojoResult)>(
          static_cast<mojo::Callback<void(MojoResult)>::Runnable*>(runner)));
  MojoAsyncWaitID wait_id = async_waiter_->AsyncWait(
      handle, signals, deadline, &AsyncCallbackForwarder, callback.get());
  callbacks_[wait_id] = callback;
  runner->set_wait_id(wait_id);
  return wait_id;
}

void PythonAsyncWaiter::CancelWait(MojoAsyncWaitID wait_id) {
  if (callbacks_.find(wait_id) != callbacks_.end()) {
    async_waiter_->CancelWait(wait_id);
    callbacks_.erase(wait_id);
  }
}

}  // namespace python
}  // namespace mojo
