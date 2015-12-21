// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DATA_PIPE_PRODUCER_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_DATA_PIPE_PRODUCER_DISPATCHER_H_

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

// This is the |Dispatcher| implementation for the producer handle for data
// pipes (created by the Mojo primitive |MojoCreateDataPipe()|). This class is
// thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipeProducerDispatcher final
    : public Dispatcher, public RawChannel::Delegate {
 public:
  static scoped_refptr<DataPipeProducerDispatcher> Create(
      const MojoCreateDataPipeOptions& options) {
    return make_scoped_refptr(new DataPipeProducerDispatcher(options));
  }

  // Must be called before any other methods.
  void Init(ScopedPlatformHandle message_pipe,
            char* serialized_write_buffer, size_t serialized_write_buffer_size);

  // |Dispatcher| public methods:
  Type GetType() const override;

  // The "opposite" of |SerializeAndClose()|. (Typically this is called by
  // |Dispatcher::Deserialize()|.)
  static scoped_refptr<DataPipeProducerDispatcher>
  Deserialize(const void* source,
              size_t size,
              PlatformHandleVector* platform_handles);

 private:
  DataPipeProducerDispatcher(const MojoCreateDataPipeOptions& options);
  ~DataPipeProducerDispatcher() override;

  void InitOnIO();
  void CloseOnIO();

  // |Dispatcher| protected methods:
  void CancelAllAwakablesNoLock() override;
  void CloseImplNoLock() override;
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseImplNoLock()
      override;
  MojoResult WriteDataImplNoLock(const void* elements,
                                 uint32_t* num_bytes,
                                 MojoWriteDataFlags flags) override;
  MojoResult BeginWriteDataImplNoLock(void** buffer,
                                      uint32_t* buffer_num_bytes,
                                      MojoWriteDataFlags flags) override;
  MojoResult EndWriteDataImplNoLock(uint32_t num_bytes_written) override;
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

  bool InTwoPhaseWrite() const;
  bool WriteDataIntoMessages(const void* elements, uint32_t num_bytes);

  // See comment in MessagePipeDispatcher for this method.
  void SerializeInternal();

  MojoCreateDataPipeOptions options_;

  // Protected by |lock()|:
  RawChannel* channel_;  // This will be null if closed.

  AwakableList awakable_list_;

  // If DispatcherTransport is created. Must be set before lock() is called to
  // avoid deadlocks with RawChannel calling us.
  base::Lock started_transport_;

  bool error_;

  bool serialized_;
  ScopedPlatformHandle serialized_platform_handle_;
  std::vector<char> serialized_write_buffer_;
  std::vector<char> two_phase_data_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(DataPipeProducerDispatcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DATA_PIPE_PRODUCER_DISPATCHER_H_
