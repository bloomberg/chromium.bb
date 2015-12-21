// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DATA_PIPE_H_
#define MOJO_EDK_SYSTEM_DATA_PIPE_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace edk {
class RawChannel;

// Shared code between DataPipeConsumerDispatcher and
// DataPipeProducerDispatcher.
class MOJO_SYSTEM_IMPL_EXPORT DataPipe {
 public:
  // The default options for |MojoCreateDataPipe()|. (Real uses should obtain
  // this via |ValidateCreateOptions()| with a null |in_options|; this is
  // exposed directly for testing convenience.)
  static MojoCreateDataPipeOptions GetDefaultCreateOptions();

  // Validates and/or sets default options for |MojoCreateDataPipeOptions|. If
  // non-null, |in_options| must point to a struct of at least
  // |in_options->struct_size| bytes. |out_options| must point to a (current)
  // |MojoCreateDataPipeOptions| and will be entirely overwritten on success (it
  // may be partly overwritten on failure).
  static MojoResult ValidateCreateOptions(
      const MojoCreateDataPipeOptions* in_options,
      MojoCreateDataPipeOptions* out_options);

  // Helper methods used by DataPipeConsumerDispatcher and
  // DataPipeProducerDispatcher for serialization and deserialization.
  static void StartSerialize(bool have_channel_handle,
                             bool have_shared_memory,
                             size_t* max_size,
                             size_t* max_platform_handles);
  static void EndSerialize(const MojoCreateDataPipeOptions& options,
                           ScopedPlatformHandle channel_handle,
                           ScopedPlatformHandle shared_memory_handle,
                           size_t shared_memory_size,
                           void* destination,
                           size_t* actual_size,
                           PlatformHandleVector* platform_handles);
  static ScopedPlatformHandle Deserialize(
      const void* source,
      size_t size,
      PlatformHandleVector* platform_handles,
      MojoCreateDataPipeOptions* options,
      ScopedPlatformHandle* shared_memory_handle,
      size_t* shared_memory_size);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DATA_PIPE_H_
