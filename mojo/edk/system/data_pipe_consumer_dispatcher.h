// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "mojo/edk/system/awakable_list.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/raw_channel.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace edk {

// This is the |Dispatcher| implementation for the consumer handle for data
// pipes (created by the Mojo primitive |MojoCreateDataPipe()|). This class is
// thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipeConsumerDispatcher final
    : public Dispatcher, public RawChannel::Delegate {
 public:
  static scoped_refptr<DataPipeConsumerDispatcher> Create(
      const MojoCreateDataPipeOptions& options) {
    return make_scoped_refptr(new DataPipeConsumerDispatcher(options));
  }

  // Must be called before any other methods.
  void Init(ScopedPlatformHandle message_pipe,
            char* serialized_read_buffer, size_t serialized_read_buffer_size);

  // |Dispatcher| public methods:
  Type GetType() const override;

  // The "opposite" of |SerializeAndClose()|. (Typically this is called by
  // |Dispatcher::Deserialize()|.)
  static scoped_refptr<DataPipeConsumerDispatcher>
  Deserialize(const void* source,
              size_t size,
              PlatformHandleVector* platform_handles);

 private:
  DataPipeConsumerDispatcher(const MojoCreateDataPipeOptions& options);
  ~DataPipeConsumerDispatcher() override;

  void InitOnIO();
  void CloseOnIO();

  // |Dispatcher| protected methods:
  void CancelAllAwakablesNoLock() override;
  void CloseImplNoLock() override;
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseImplNoLock()
      override;
  MojoResult ReadDataImplNoLock(void* elements,
                                uint32_t* num_bytes,
                                MojoReadDataFlags flags) override;
  MojoResult BeginReadDataImplNoLock(const void** buffer,
                                     uint32_t* buffer_num_bytes,
                                     MojoReadDataFlags flags) override;
  MojoResult EndReadDataImplNoLock(uint32_t num_bytes_read) override;
  HandleSignalsState GetHandleSignalsStateImplNoLock() const override;
  MojoResult AddAwakableImplNoLock(Awakable* awakable,
                                   MojoHandleSignals signals,
                                   uintptr_t context,
                                   HandleSignalsState* signals_state) override;
  void RemoveAwakableImplNoLock(Awakable* awakable,
                                HandleSignalsState* signals_state) override;
  void StartSerializeImplNoLock(size_t* max_size,
                                size_t* max_platform_handles) override;
  bool EndSerializeAndCloseImplNoLock(
      void* destination,
      size_t* actual_size,
      PlatformHandleVector* platform_handles) override;
  void TransportStarted() override;
  void TransportEnded() override;
  bool IsBusyNoLock() const override;

  // |RawChannel::Delegate methods:
  void OnReadMessage(
    const MessageInTransit::View& message_view,
    ScopedPlatformHandleVectorPtr platform_handles) override;
  void OnError(Error error) override;

  // See comment in MessagePipeDispatcher for this method.
  void SerializeInternal();

  MojoCreateDataPipeOptions options_;

  // Protected by |lock()|:
  RawChannel* channel_;  // This will be null if closed.

  // Queue of incoming messages.
  std::vector<char> data_;
  AwakableList awakable_list_;

  // If DispatcherTransport is created. Must be set before lock() is called to
  // avoid deadlocks with RawChannel calling us.
  base::Lock started_transport_;

  bool calling_init_;

  bool in_two_phase_read_;
  uint32_t two_phase_max_bytes_read_;
  // If we get data from the channel while we're in two-phase read, we can't
  // resize data_ since it's being used. So instead we store it temporarly.
  std::vector<char> data_received_during_two_phase_read_;

  bool error_;

  bool serialized_;
  std::vector<char> serialized_read_buffer_;
  ScopedPlatformHandle serialized_platform_handle_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(DataPipeConsumerDispatcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_
