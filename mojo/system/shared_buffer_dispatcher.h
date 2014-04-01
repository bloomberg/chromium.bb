// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_SHARED_BUFFER_DISPATCHER_H_
#define MOJO_SYSTEM_SHARED_BUFFER_DISPATCHER_H_

#include "base/macros.h"
#include "mojo/system/raw_shared_buffer.h"
#include "mojo/system/simple_dispatcher.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// TODO(vtl): We derive from SimpleDispatcher, even though we don't currently
// have anything that's waitable. I want to add a "transferrable" wait flag.
class MOJO_SYSTEM_IMPL_EXPORT SharedBufferDispatcher : public SimpleDispatcher {
 public:
  // Validates and/or sets default options. If non-null, |in_options| must point
  // to a struct of at least |in_options->struct_size| bytes. |out_options| must
  // point to a (current) |MojoCreateSharedBufferOptions| and will be entirely
  // overwritten on success (it may be partly overwritten on failure).
  static MojoResult ValidateOptions(
      const MojoCreateSharedBufferOptions* in_options,
      MojoCreateSharedBufferOptions* out_options);

  // Static factory method: |validated_options| must be validated (obviously).
  // On failure, |*result| will be left as-is.
  static MojoResult Create(
      const MojoCreateSharedBufferOptions& validated_options,
      uint64_t num_bytes,
      scoped_refptr<SharedBufferDispatcher>* result);

  // |Dispatcher| public methods:
  virtual Type GetType() const OVERRIDE;

 private:
  explicit SharedBufferDispatcher(
      scoped_refptr<RawSharedBuffer> shared_buffer_);
  virtual ~SharedBufferDispatcher();

  // |Dispatcher| protected methods:
  virtual void CloseImplNoLock() OVERRIDE;
  virtual scoped_refptr<Dispatcher>
      CreateEquivalentDispatcherAndCloseImplNoLock() OVERRIDE;
  virtual MojoResult DuplicateBufferHandleImplNoLock(
      const MojoDuplicateBufferHandleOptions* options,
      scoped_refptr<Dispatcher>* new_dispatcher) OVERRIDE;
  virtual MojoResult MapBufferImplNoLock(
      uint64_t offset,
      uint64_t num_bytes,
      MojoMapBufferFlags flags,
      scoped_ptr<RawSharedBufferMapping>* mapping) OVERRIDE;

  // |SimpleDispatcher| methods:
  virtual MojoWaitFlags SatisfiedFlagsNoLock() const OVERRIDE;
  virtual MojoWaitFlags SatisfiableFlagsNoLock() const OVERRIDE;

  scoped_refptr<RawSharedBuffer> shared_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SharedBufferDispatcher);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_SHARED_BUFFER_DISPATCHER_H_
