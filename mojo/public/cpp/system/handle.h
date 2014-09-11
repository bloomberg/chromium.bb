// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_HANDLE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_HANDLE_H_

#include <assert.h>
#include <limits>

#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

// OVERVIEW
//
// |Handle| and |...Handle|:
//
// |Handle| is a simple, copyable wrapper for the C type |MojoHandle| (which is
// just an integer). Its purpose is to increase type-safety, not provide
// lifetime management. For the same purpose, we have trivial *subclasses* of
// |Handle|, e.g., |MessagePipeHandle| and |DataPipeProducerHandle|. |Handle|
// and its subclasses impose *no* extra overhead over using |MojoHandle|s
// directly.
//
// Note that though we provide constructors for |Handle|/|...Handle| from a
// |MojoHandle|, we do not provide, e.g., a constructor for |MessagePipeHandle|
// from a |Handle|. This is for type safety: If we did, you'd then be able to
// construct a |MessagePipeHandle| from, e.g., a |DataPipeProducerHandle| (since
// it's a |Handle|).
//
// |ScopedHandleBase| and |Scoped...Handle|:
//
// |ScopedHandleBase<HandleType>| is a templated scoped wrapper, for the handle
// types above (in the same sense that a C++11 |unique_ptr<T>| is a scoped
// wrapper for a |T*|). It provides lifetime management, closing its owned
// handle on destruction. It also provides (emulated) move semantics, again
// along the lines of C++11's |unique_ptr| (and exactly like Chromium's
// |scoped_ptr|).
//
// |ScopedHandle| is just (a typedef of) a |ScopedHandleBase<Handle>|.
// Similarly, |ScopedMessagePipeHandle| is just a
// |ScopedHandleBase<MessagePipeHandle>|. Etc. Note that a
// |ScopedMessagePipeHandle| is *not* a (subclass of) |ScopedHandle|.
//
// Wrapper functions:
//
// We provide simple wrappers for the |Mojo...()| functions (in
// mojo/public/c/system/core.h -- see that file for details on individual
// functions).
//
// The general guideline is functions that imply ownership transfer of a handle
// should take (or produce) an appropriate |Scoped...Handle|, while those that
// don't take a |...Handle|. For example, |CreateMessagePipe()| has two
// |ScopedMessagePipe| "out" parameters, whereas |Wait()| and |WaitMany()| take
// |Handle| parameters. Some, have both: e.g., |DuplicatedBuffer()| takes a
// suitable (unscoped) handle (e.g., |SharedBufferHandle|) "in" parameter and
// produces a suitable scoped handle (e.g., |ScopedSharedBufferHandle| a.k.a.
// |ScopedHandleBase<SharedBufferHandle>|) as an "out" parameter.
//
// An exception are some of the |...Raw()| functions. E.g., |CloseRaw()| takes a
// |Handle|, leaving the user to discard the handle.
//
// More significantly, |WriteMessageRaw()| exposes the full API complexity of
// |MojoWriteMessage()| (but doesn't require any extra overhead). It takes a raw
// array of |Handle|s as input, and takes ownership of them (i.e., invalidates
// them) on *success* (but not on failure). There are a number of reasons for
// this. First, C++03 |std::vector|s cannot contain the move-only
// |Scoped...Handle|s. Second, |std::vector|s impose extra overhead
// (necessitating heap-allocation of the buffer). Third, |std::vector|s wouldn't
// provide the desired level of flexibility/safety: a vector of handles would
// have to be all of the same type (probably |Handle|/|ScopedHandle|). Fourth,
// it's expected to not be used directly, but instead be used by generated
// bindings.
//
// Other |...Raw()| functions expose similar rough edges, e.g., dealing with raw
// pointers (and lengths) instead of taking |std::vector|s or similar.

// ScopedHandleBase ------------------------------------------------------------

// Scoper for the actual handle types defined further below. It's move-only,
// like the C++11 |unique_ptr|.
template <class HandleType>
class ScopedHandleBase {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(ScopedHandleBase, RValue)

 public:
  ScopedHandleBase() {}
  explicit ScopedHandleBase(HandleType handle) : handle_(handle) {}
  ~ScopedHandleBase() { CloseIfNecessary(); }

  template <class CompatibleHandleType>
  explicit ScopedHandleBase(ScopedHandleBase<CompatibleHandleType> other)
      : handle_(other.release()) {
  }

  // Move-only constructor and operator=.
  ScopedHandleBase(RValue other) : handle_(other.object->release()) {}
  ScopedHandleBase& operator=(RValue other) {
    if (other.object != this) {
      CloseIfNecessary();
      handle_ = other.object->release();
    }
    return *this;
  }

  const HandleType& get() const { return handle_; }

  template <typename PassedHandleType>
  static ScopedHandleBase<HandleType> From(
      ScopedHandleBase<PassedHandleType> other) {
    MOJO_COMPILE_ASSERT(
        sizeof(static_cast<PassedHandleType*>(static_cast<HandleType*>(0))),
        HandleType_is_not_a_subtype_of_PassedHandleType);
    return ScopedHandleBase<HandleType>(
        static_cast<HandleType>(other.release().value()));
  }

  void swap(ScopedHandleBase& other) {
    handle_.swap(other.handle_);
  }

  HandleType release() MOJO_WARN_UNUSED_RESULT {
    HandleType rv;
    rv.swap(handle_);
    return rv;
  }

  void reset(HandleType handle = HandleType()) {
    CloseIfNecessary();
    handle_ = handle;
  }

  bool is_valid() const {
    return handle_.is_valid();
  }

 private:
  void CloseIfNecessary() {
    if (!handle_.is_valid())
      return;
    MojoResult result MOJO_ALLOW_UNUSED = MojoClose(handle_.value());
    assert(result == MOJO_RESULT_OK);
  }

  HandleType handle_;
};

template <typename HandleType>
inline ScopedHandleBase<HandleType> MakeScopedHandle(HandleType handle) {
  return ScopedHandleBase<HandleType>(handle);
}

// Handle ----------------------------------------------------------------------

const MojoHandle kInvalidHandleValue = MOJO_HANDLE_INVALID;

// Wrapper base class for |MojoHandle|.
class Handle {
 public:
  Handle() : value_(kInvalidHandleValue) {}
  explicit Handle(MojoHandle value) : value_(value) {}
  ~Handle() {}

  void swap(Handle& other) {
    MojoHandle temp = value_;
    value_ = other.value_;
    other.value_ = temp;
  }

  bool is_valid() const {
    return value_ != kInvalidHandleValue;
  }

  const MojoHandle& value() const { return value_; }
  MojoHandle* mutable_value() { return &value_; }
  void set_value(MojoHandle value) { value_ = value; }

 private:
  MojoHandle value_;

  // Copying and assignment allowed.
};

// Should have zero overhead.
MOJO_COMPILE_ASSERT(sizeof(Handle) == sizeof(MojoHandle),
                    bad_size_for_cpp_Handle);

// The scoper should also impose no more overhead.
typedef ScopedHandleBase<Handle> ScopedHandle;
MOJO_COMPILE_ASSERT(sizeof(ScopedHandle) == sizeof(Handle),
                    bad_size_for_cpp_ScopedHandle);

inline MojoResult Wait(Handle handle,
                       MojoHandleSignals signals,
                       MojoDeadline deadline) {
  return MojoWait(handle.value(), signals, deadline);
}

// |HandleVectorType| and |FlagsVectorType| should be similar enough to
// |std::vector<Handle>| and |std::vector<MojoHandleSignals>|, respectively:
//  - They should have a (const) |size()| method that returns an unsigned type.
//  - They must provide contiguous storage, with access via (const) reference to
//    that storage provided by a (const) |operator[]()| (by reference).
template <class HandleVectorType, class FlagsVectorType>
inline MojoResult WaitMany(const HandleVectorType& handles,
                           const FlagsVectorType& signals,
                           MojoDeadline deadline) {
  if (signals.size() != handles.size())
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (handles.size() > std::numeric_limits<uint32_t>::max())
    return MOJO_RESULT_OUT_OF_RANGE;

  if (handles.size() == 0)
    return MojoWaitMany(NULL, NULL, 0, deadline);

  const Handle& first_handle = handles[0];
  const MojoHandleSignals& first_signals = signals[0];
  return MojoWaitMany(
      reinterpret_cast<const MojoHandle*>(&first_handle),
      reinterpret_cast<const MojoHandleSignals*>(&first_signals),
      static_cast<uint32_t>(handles.size()),
      deadline);
}

// |Close()| takes ownership of the handle, since it'll invalidate it.
// Note: There's nothing to do, since the argument will be destroyed when it
// goes out of scope.
template <class HandleType>
inline void Close(ScopedHandleBase<HandleType> /*handle*/) {}

// Most users should typically use |Close()| (above) instead.
inline MojoResult CloseRaw(Handle handle) {
  return MojoClose(handle.value());
}

// Strict weak ordering, so that |Handle|s can be used as keys in |std::map|s,
inline bool operator<(const Handle a, const Handle b) {
  return a.value() < b.value();
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_HANDLE_H_
