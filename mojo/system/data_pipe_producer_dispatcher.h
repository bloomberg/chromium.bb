// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_DATA_PIPE_PRODUCER_DISPATCHER_H_
#define MOJO_SYSTEM_DATA_PIPE_PRODUCER_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class DataPipe;

// This is the |Dispatcher| implementation for the producer handle for data
// pipes (created by the Mojo primitive |MojoCreateDataPipe()|). This class is
// thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipeProducerDispatcher : public Dispatcher {
 public:
  DataPipeProducerDispatcher();

  // Must be called before any other methods.
  void Init(scoped_refptr<DataPipe> data_pipe);

 private:
  friend class base::RefCountedThreadSafe<DataPipeProducerDispatcher>;
  virtual ~DataPipeProducerDispatcher();

  // |Dispatcher| implementation/overrides:
  virtual void CancelAllWaitersNoLock() OVERRIDE;
  virtual MojoResult CloseImplNoLock() OVERRIDE;
  virtual MojoResult WriteDataImplNoLock(const void* elements,
                                         uint32_t* num_bytes,
                                         MojoWriteDataFlags flags) OVERRIDE;
  virtual MojoResult BeginWriteDataImplNoLock(
      void** buffer,
      uint32_t* buffer_num_bytes,
      MojoWriteDataFlags flags) OVERRIDE;
  virtual MojoResult EndWriteDataImplNoLock(
      uint32_t num_bytes_written) OVERRIDE;
  virtual MojoResult AddWaiterImplNoLock(Waiter* waiter,
                                         MojoWaitFlags flags,
                                         MojoResult wake_result) OVERRIDE;
  virtual void RemoveWaiterImplNoLock(Waiter* waiter) OVERRIDE;
  virtual bool IsBusyNoLock() OVERRIDE;
  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE;

  // Protected by |lock()|:
  scoped_refptr<DataPipe> data_pipe_;  // This will be null if closed.

  DISALLOW_COPY_AND_ASSIGN(DataPipeProducerDispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_DATA_PIPE_PRODUCER_DISPATCHER_H_
