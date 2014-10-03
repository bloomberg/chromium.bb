// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_
#define MOJO_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class DataPipe;

// This is the |Dispatcher| implementation for the consumer handle for data
// pipes (created by the Mojo primitive |MojoCreateDataPipe()|). This class is
// thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipeConsumerDispatcher : public Dispatcher {
 public:
  DataPipeConsumerDispatcher();

  // Must be called before any other methods.
  void Init(scoped_refptr<DataPipe> data_pipe);

  // |Dispatcher| public methods:
  virtual Type GetType() const override;

 private:
  virtual ~DataPipeConsumerDispatcher();

  // |Dispatcher| protected methods:
  virtual void CancelAllWaitersNoLock() override;
  virtual void CloseImplNoLock() override;
  virtual scoped_refptr<Dispatcher>
  CreateEquivalentDispatcherAndCloseImplNoLock() override;
  virtual MojoResult ReadDataImplNoLock(UserPointer<void> elements,
                                        UserPointer<uint32_t> num_bytes,
                                        MojoReadDataFlags flags) override;
  virtual MojoResult BeginReadDataImplNoLock(
      UserPointer<const void*> buffer,
      UserPointer<uint32_t> buffer_num_bytes,
      MojoReadDataFlags flags) override;
  virtual MojoResult EndReadDataImplNoLock(uint32_t num_bytes_read) override;
  virtual HandleSignalsState GetHandleSignalsStateImplNoLock() const override;
  virtual MojoResult AddWaiterImplNoLock(
      Waiter* waiter,
      MojoHandleSignals signals,
      uint32_t context,
      HandleSignalsState* signals_state) override;
  virtual void RemoveWaiterImplNoLock(
      Waiter* waiter,
      HandleSignalsState* signals_state) override;
  virtual bool IsBusyNoLock() const override;

  // Protected by |lock()|:
  scoped_refptr<DataPipe> data_pipe_;  // This will be null if closed.

  DISALLOW_COPY_AND_ASSIGN(DataPipeConsumerDispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_DATA_PIPE_CONSUMER_DISPATCHER_H_
