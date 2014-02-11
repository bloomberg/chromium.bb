// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SYSTEM_CORE_CPP_H_
#define MOJO_PUBLIC_SYSTEM_CORE_CPP_H_

#include <assert.h>
#include <stddef.h>

#include <limits>

#include "mojo/public/system/core.h"
#include "mojo/public/system/macros.h"
#include "mojo/public/system/system_export.h"

namespace mojo {

// Standalone functions --------------------------------------------------------

inline MojoTimeTicks GetTimeTicksNow() {
  return MojoGetTimeTicksNow();
}

// ScopedHandleBase ------------------------------------------------------------

// Scoper for the actual handle types defined further below. It's move-only,
// like the C++11 |unique_ptr|.
template <class HandleType>
class ScopedHandleBase {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(ScopedHandleBase, RValue);

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
    handle_ = other.object->release();
    return *this;
  }

  const HandleType& get() const { return handle_; }

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
  Handle() : value_(MOJO_HANDLE_INVALID) {}
  explicit Handle(MojoHandle value) : value_(value) {}
  ~Handle() {}

  void swap(Handle& other) {
    MojoHandle temp = value_;
    value_ = other.value_;
    other.value_ = temp;
  }

  bool is_valid() const {
    return value_ != MOJO_HANDLE_INVALID;
  }

  MojoHandle value() const { return value_; }
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

inline MojoResult Wait(const Handle& handle,
                       MojoWaitFlags flags,
                       MojoDeadline deadline) {
  return MojoWait(handle.value(), flags, deadline);
}

// |HandleVectorType| and |FlagsVectorType| should be similar enough to
// |std::vector<Handle>| and |std::vector<MojoWaitFlags>|, respectively:
//  - They should have a (const) |size()| method that returns an unsigned type.
//  - They must provide contiguous storage, with access via (const) reference to
//    that storage provided by a (const) |operator[]()| (by reference).
template <class HandleVectorType, class FlagsVectorType>
inline MojoResult WaitMany(const HandleVectorType& handles,
                           const FlagsVectorType& flags,
                           MojoDeadline deadline) {
  if (flags.size() != handles.size())
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (handles.size() > std::numeric_limits<uint32_t>::max())
    return MOJO_RESULT_OUT_OF_RANGE;

  if (handles.size() == 0)
    return MojoWaitMany(NULL, NULL, 0, deadline);

  const Handle& first_handle = handles[0];
  const MojoWaitFlags& first_flag = flags[0];
  return MojoWaitMany(reinterpret_cast<const MojoHandle*>(&first_handle),
                      reinterpret_cast<const MojoWaitFlags*>(&first_flag),
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
// etc.
inline bool operator<(const Handle& a, const Handle& b) {
  return a.value() < b.value();
}

// MessagePipeHandle -----------------------------------------------------------

class MessagePipeHandle : public Handle {
 public:
  MessagePipeHandle() {}
  explicit MessagePipeHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

MOJO_COMPILE_ASSERT(sizeof(MessagePipeHandle) == sizeof(Handle),
                    bad_size_for_cpp_MessagePipeHandle);

typedef ScopedHandleBase<MessagePipeHandle> ScopedMessagePipeHandle;
MOJO_COMPILE_ASSERT(sizeof(ScopedMessagePipeHandle) ==
                        sizeof(MessagePipeHandle),
                    bad_size_for_cpp_ScopedMessagePipeHandle);

// TODO(vtl): In C++11, we could instead return a pair of
// |ScopedMessagePipeHandle|s.
inline void CreateMessagePipe(ScopedMessagePipeHandle* message_pipe0,
                              ScopedMessagePipeHandle* message_pipe1) {
  assert(message_pipe0);
  assert(message_pipe1);
  MessagePipeHandle h0;
  MessagePipeHandle h1;
  MojoResult result MOJO_ALLOW_UNUSED =
      MojoCreateMessagePipe(h0.mutable_value(), h1.mutable_value());
  assert(result == MOJO_RESULT_OK);
  message_pipe0->reset(h0);
  message_pipe1->reset(h1);
}

// A wrapper class that automatically creates a message pipe and owns both
// handles.
class MessagePipe {
 public:
  MessagePipe();
  ~MessagePipe();

  ScopedMessagePipeHandle handle0;
  ScopedMessagePipeHandle handle1;
};

inline MessagePipe::MessagePipe() {
  CreateMessagePipe(&handle0, &handle1);
}

inline MessagePipe::~MessagePipe() {
}

// These "raw" versions fully expose the underlying API, but don't help with
// ownership of handles (especially when writing messages).
// TODO(vtl): Write "baked" versions.
inline MojoResult WriteMessageRaw(MessagePipeHandle message_pipe,
                                  const void* bytes,
                                  uint32_t num_bytes,
                                  const MojoHandle* handles,
                                  uint32_t num_handles,
                                  MojoWriteMessageFlags flags) {
  return MojoWriteMessage(message_pipe.value(), bytes, num_bytes, handles,
                          num_handles, flags);
}

inline MojoResult ReadMessageRaw(MessagePipeHandle message_pipe,
                                 void* bytes,
                                 uint32_t* num_bytes,
                                 MojoHandle* handles,
                                 uint32_t* num_handles,
                                 MojoReadMessageFlags flags) {
  return MojoReadMessage(message_pipe.value(), bytes, num_bytes, handles,
                         num_handles, flags);
}

// DataPipeProducerHandle and DataPipeConsumerHandle ---------------------------

class DataPipeProducerHandle : public Handle {
 public:
  DataPipeProducerHandle() {}
  explicit DataPipeProducerHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

MOJO_COMPILE_ASSERT(sizeof(DataPipeProducerHandle) == sizeof(Handle),
                    bad_size_for_cpp_DataPipeProducerHandle);

typedef ScopedHandleBase<DataPipeProducerHandle> ScopedDataPipeProducerHandle;
MOJO_COMPILE_ASSERT(sizeof(ScopedDataPipeProducerHandle) ==
                        sizeof(DataPipeProducerHandle),
                    bad_size_for_cpp_ScopedDataPipeProducerHandle);

class DataPipeConsumerHandle : public Handle {
 public:
  DataPipeConsumerHandle() {}
  explicit DataPipeConsumerHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

MOJO_COMPILE_ASSERT(sizeof(DataPipeConsumerHandle) == sizeof(Handle),
                    bad_size_for_cpp_DataPipeConsumerHandle);

typedef ScopedHandleBase<DataPipeConsumerHandle> ScopedDataPipeConsumerHandle;
MOJO_COMPILE_ASSERT(sizeof(ScopedDataPipeConsumerHandle) ==
                        sizeof(DataPipeConsumerHandle),
                    bad_size_for_cpp_ScopedDataPipeConsumerHandle);

inline MojoResult CreateDataPipe(
    const MojoCreateDataPipeOptions* options,
    ScopedDataPipeProducerHandle* data_pipe_producer,
    ScopedDataPipeConsumerHandle* data_pipe_consumer) {
  assert(data_pipe_producer);
  assert(data_pipe_consumer);
  DataPipeProducerHandle producer_handle;
  DataPipeConsumerHandle consumer_handle;
  MojoResult rv = MojoCreateDataPipe(options,
                                     producer_handle.mutable_value(),
                                     consumer_handle.mutable_value());
  // Reset even on failure (reduces the chances that a "stale"/incorrect handle
  // will be used).
  data_pipe_producer->reset(producer_handle);
  data_pipe_consumer->reset(consumer_handle);
  return rv;
}

inline MojoResult WriteDataRaw(DataPipeProducerHandle data_pipe_producer,
                               const void* elements,
                               uint32_t* num_bytes,
                               MojoWriteDataFlags flags) {
  return MojoWriteData(data_pipe_producer.value(), elements, num_bytes, flags);
}

inline MojoResult BeginWriteDataRaw(DataPipeProducerHandle data_pipe_producer,
                                    void** buffer,
                                    uint32_t* buffer_num_bytes,
                                    MojoWriteDataFlags flags) {
  return MojoBeginWriteData(data_pipe_producer.value(), buffer,
                            buffer_num_bytes, flags);
}

inline MojoResult EndWriteDataRaw(DataPipeProducerHandle data_pipe_producer,
                                  uint32_t num_bytes_written) {
  return MojoEndWriteData(data_pipe_producer.value(), num_bytes_written);
}

inline MojoResult ReadDataRaw(DataPipeConsumerHandle data_pipe_consumer,
                              void* elements,
                              uint32_t* num_bytes,
                              MojoReadDataFlags flags) {
  return MojoReadData(data_pipe_consumer.value(), elements, num_bytes, flags);
}

inline MojoResult BeginReadDataRaw(DataPipeConsumerHandle data_pipe_consumer,
                                   const void** buffer,
                                   uint32_t* buffer_num_bytes,
                                   MojoReadDataFlags flags) {
  return MojoBeginReadData(data_pipe_consumer.value(), buffer, buffer_num_bytes,
                           flags);
}

inline MojoResult EndReadDataRaw(DataPipeConsumerHandle data_pipe_consumer,
                                 uint32_t num_bytes_read) {
  return MojoEndReadData(data_pipe_consumer.value(), num_bytes_read);
}

// A wrapper class that automatically creates a data pipe and owns both handles.
// TODO(vtl): Make an even more friendly version? (Maybe templatized for a
// particular type instead of some "element"? Maybe functions that take
// vectors?)
class DataPipe {
 public:
  DataPipe();
  explicit DataPipe(const MojoCreateDataPipeOptions& options);
  ~DataPipe();

  ScopedDataPipeProducerHandle producer_handle;
  ScopedDataPipeConsumerHandle consumer_handle;
};

inline DataPipe::DataPipe() {
  CreateDataPipe(NULL, &producer_handle, &consumer_handle);
}

inline DataPipe::DataPipe(const MojoCreateDataPipeOptions& options) {
  CreateDataPipe(&options, &producer_handle, &consumer_handle);
}

inline DataPipe::~DataPipe() {
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_SYSTEM_CORE_CPP_H_
