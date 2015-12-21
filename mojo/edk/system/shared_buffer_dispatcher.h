// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_SHARED_BUFFER_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_SHARED_BUFFER_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/system/simple_dispatcher.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

namespace edk {
class PlatformSupport;

// TODO(vtl): We derive from SimpleDispatcher, even though we don't currently
// have anything that's waitable. I want to add a "transferrable" wait flag
// (which would entail overriding |GetHandleSignalsStateImplNoLock()|, etc.).
class MOJO_SYSTEM_IMPL_EXPORT SharedBufferDispatcher final
    : public SimpleDispatcher {
 public:
  // The default options to use for |MojoCreateSharedBuffer()|. (Real uses
  // should obtain this via |ValidateCreateOptions()| with a null |in_options|;
  // this is exposed directly for testing convenience.)
  static const MojoCreateSharedBufferOptions kDefaultCreateOptions;

  // Validates and/or sets default options for |MojoCreateSharedBufferOptions|.
  // If non-null, |in_options| must point to a struct of at least
  // |in_options->struct_size| bytes. |out_options| must point to a (current)
  // |MojoCreateSharedBufferOptions| and will be entirely overwritten on success
  // (it may be partly overwritten on failure).
  static MojoResult ValidateCreateOptions(
      const MojoCreateSharedBufferOptions* in_options,
      MojoCreateSharedBufferOptions* out_options);

  // Static factory method: |validated_options| must be validated (obviously).
  // On failure, |*result| will be left as-is.
  // TODO(vtl): This should probably be made to return a scoped_refptr and have
  // a MojoResult out parameter instead.
  static MojoResult Create(
      PlatformSupport* platform_support,
      const MojoCreateSharedBufferOptions& validated_options,
      uint64_t num_bytes,
      scoped_refptr<SharedBufferDispatcher>* result);

  // |Dispatcher| public methods:
  Type GetType() const override;

  // The "opposite" of |SerializeAndClose()|. (Typically this is called by
  // |Dispatcher::Deserialize()|.)
  static scoped_refptr<SharedBufferDispatcher> Deserialize(
      const void* source,
      size_t size,
      PlatformHandleVector* platform_handles);

 private:
  static scoped_refptr<SharedBufferDispatcher> CreateInternal(
      scoped_refptr<PlatformSharedBuffer> shared_buffer) {
    return make_scoped_refptr(
        new SharedBufferDispatcher(std::move(shared_buffer)));
  }

  explicit SharedBufferDispatcher(
      scoped_refptr<PlatformSharedBuffer> shared_buffer);
  ~SharedBufferDispatcher() override;

  // Validates and/or sets default options for
  // |MojoDuplicateBufferHandleOptions|. If non-null, |in_options| must point to
  // a struct of at least |in_options->struct_size| bytes. |out_options| must
  // point to a (current) |MojoDuplicateBufferHandleOptions| and will be
  // entirely overwritten on success (it may be partly overwritten on failure).
  static MojoResult ValidateDuplicateOptions(
      const MojoDuplicateBufferHandleOptions* in_options,
      MojoDuplicateBufferHandleOptions* out_options);

  // |Dispatcher| protected methods:
  void CloseImplNoLock() override;
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseImplNoLock()
      override;
  MojoResult DuplicateBufferHandleImplNoLock(
      const MojoDuplicateBufferHandleOptions* options,
      scoped_refptr<Dispatcher>* new_dispatcher) override;
  MojoResult MapBufferImplNoLock(
      uint64_t offset,
      uint64_t num_bytes,
      MojoMapBufferFlags flags,
      scoped_ptr<PlatformSharedBufferMapping>* mapping) override;
  void StartSerializeImplNoLock(size_t* max_size,
                                size_t* max_platform_handles) override;
  bool EndSerializeAndCloseImplNoLock(
      void* destination,
      size_t* actual_size,
      PlatformHandleVector* platform_handles) override;

  scoped_refptr<PlatformSharedBuffer> shared_buffer_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(SharedBufferDispatcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_SHARED_BUFFER_DISPATCHER_H_
