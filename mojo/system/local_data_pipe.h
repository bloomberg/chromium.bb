// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_LOCAL_DATA_PIPE_H_
#define MOJO_SYSTEM_LOCAL_DATA_PIPE_H_

#include "base/basictypes.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/system/data_pipe.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// |LocalDataPipe| is a subclass that "implements" |DataPipe| for data pipes
// whose producer and consumer are both local. This class is thread-safe (with
// protection provided by |DataPipe|'s |lock_|.
class MOJO_SYSTEM_IMPL_EXPORT LocalDataPipe : public DataPipe {
 public:
  LocalDataPipe();

  MojoResult Init(const MojoCreateDataPipeOptions* options);

 private:
  friend class base::RefCountedThreadSafe<LocalDataPipe>;
  virtual ~LocalDataPipe();

  // |DataPipe| implementation:
  virtual void ProducerCloseImplNoLock() OVERRIDE;
  virtual MojoResult ProducerBeginWriteDataImplNoLock(
      void** buffer,
      uint32_t* buffer_num_elements,
      MojoWriteDataFlags flags) OVERRIDE;
  virtual MojoResult ProducerEndWriteDataImplNoLock(
      uint32_t num_elements_written) OVERRIDE;
  virtual MojoWaitFlags ProducerSatisfiedFlagsNoLock() OVERRIDE;
  virtual MojoWaitFlags ProducerSatisfiableFlagsNoLock() OVERRIDE;
  virtual void ConsumerCloseImplNoLock() OVERRIDE;
  virtual MojoResult ConsumerDiscardDataNoLock(uint32_t* num_elements,
                                               bool all_or_none) OVERRIDE;
  virtual MojoResult ConsumerQueryDataNoLock(uint32_t* num_elements) OVERRIDE;
  virtual MojoResult ConsumerBeginReadDataImplNoLock(
      const void** buffer,
      uint32_t* buffer_num_elements,
      MojoReadDataFlags flags) OVERRIDE;
  virtual MojoResult ConsumerEndReadDataImplNoLock(
      uint32_t num_elements_read) OVERRIDE;
  virtual MojoWaitFlags ConsumerSatisfiedFlagsNoLock() OVERRIDE;
  virtual MojoWaitFlags ConsumerSatisfiableFlagsNoLock() OVERRIDE;

  void EnsureBufferNoLock();

  // Get the maximum (single) write/read size right now (in number of elements);
  // result fits in a |uint32_t|.
  size_t GetMaxElementsToWriteNoLock();
  size_t GetMaxElementsToReadNoLock();

  // The members below are protected by |DataPipe|'s |lock_|:
  bool producer_open_;
  bool consumer_open_;

  scoped_ptr_malloc<char, base::ScopedPtrAlignedFree> buffer_;
  // Circular buffer.
  size_t buffer_first_element_index_;
  size_t buffer_current_num_elements_;

  uint32_t two_phase_max_elements_written_;
  uint32_t two_phase_max_elements_read_;

  DISALLOW_COPY_AND_ASSIGN(LocalDataPipe);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_LOCAL_DATA_PIPE_H_
