// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_H_

#include <memory>

#include "build/build_config.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource_fence.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/resource_type.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class GpuMemoryBuffer;
}

namespace viz {
namespace internal {

// The data structure used to track state of Gpu and Software-based
// resources and the service, for resources transferred
// between the two. This is an implementation detail of the resource tracking
// for client and service libraries and should not be used directly from
// external client code.
struct VIZ_COMMON_EXPORT Resource {
  enum SynchronizationState {
    // The LOCALLY_USED state is the state each resource defaults to when
    // constructed or modified or read. This state indicates that the
    // resource has not been properly synchronized and it would be an error
    // to return this resource to a client.
    LOCALLY_USED,

    // The NEEDS_WAIT state is the state that indicates a resource has been
    // modified but it also has an associated sync token assigned to it.
    // The sync token has not been waited on with the local context. When
    // a sync token arrives from a client, it is automatically initialized as
    // NEEDS_WAIT as well since we still need to wait on it before the resource
    // is synchronized on the current context. It is an error to use the
    // resource locally for reading or writing if the resource is in this state.
    NEEDS_WAIT,

    // The SYNCHRONIZED state indicates that the resource has been properly
    // synchronized locally. This can either synchronized externally (such
    // as the case of software rasterized bitmaps), or synchronized
    // internally using a sync token that has been waited upon. In the
    // former case where the resource was synchronized externally, a
    // corresponding sync token will not exist. In the latter case which was
    // synchronized from the NEEDS_WAIT state, a corresponding sync token will
    // exist which is associated with the resource. This sync token is still
    // valid and still associated with the resource and can be passed as an
    // external resource for others to wait on.
    SYNCHRONIZED,
  };

  Resource(int child_id, const TransferableResource& transferable);
  Resource(Resource&& other);
  ~Resource();

  bool is_gpu_resource_type() const { return !transferable.is_software; }

  bool needs_sync_token() const {
    return is_gpu_resource_type() && synchronization_state_ == LOCALLY_USED;
  }

  const gpu::SyncToken& sync_token() const { return sync_token_; }

  SynchronizationState synchronization_state() const {
    return synchronization_state_;
  }

  void SetLocallyUsed();
  void SetSynchronized();
  void UpdateSyncToken(const gpu::SyncToken& sync_token);
  // If true the texture-backed or GpuMemoryBuffer-backed resource needs its
  // SyncToken waited on in order to be synchronized for use.
  bool ShouldWaitSyncToken() const;
  int8_t* GetSyncTokenData();

  // This is the id of the client the resource comes from.
  const int child_id;
  // Data received from the client that describes the resource fully.
  const TransferableResource transferable;

  // The number of times the resource has been received from a client. It must
  // have this many number of references returned back to the client in order
  // for it to know it is no longer in use in the service. This is used to avoid
  // races where a resource is in flight to the service while also being
  // returned to the client. It starts with an initial count of 1, for the first
  // time the resource is received.
  int imported_count = 1;

  // The number of active users of a resource in the display compositor. While a
  // resource is in use, it will not be returned back to the client even if the
  // ResourceId is deleted.
  int lock_for_read_count = 0;
  // When true, the resource is currently being used externally. This is a
  // parallel counter to |lock_for_read_count| which can only go to 1.
  bool locked_for_external_use = false;

  // When the resource should be deleted until it is actually reaped.
  bool marked_for_deletion = false;

  // Texture id for texture-backed resources, when the mailbox is mapped into
  // a client GL id in this process.
  GLuint gl_id = 0;
  // The current min/mag filter for GL texture-backed resources. The original
  // filter as given from the client is saved in |transferable|.
  GLenum filter;
  // A pointer to the shared memory structure for software-backed resources,
  // when it is mapped into memory in this process.
  std::unique_ptr<SharedBitmap> shared_bitmap;
  // A GUID for reporting the |shared_bitmap| to memory tracing. The GUID is
  // known by other components in the system as well to give the same id for
  // this shared memory bitmap everywhere. This is empty until the resource is
  // mapped for use in the display compositor.
  base::UnguessableToken shared_bitmap_tracing_guid;

  // A fence used for accessing a gpu resource for reading, that ensures any
  // writing done to the resource has been completed. This is implemented and
  // used to implement transferring ownership of the resource from the client to
  // the service, and in the GL drawing code before reading from the texture.
  scoped_refptr<ResourceFence> read_lock_fence;

 private:
  // Tracks if a sync token needs to be waited on before using the resource.
  SynchronizationState synchronization_state_;
  // A SyncToken associated with a texture-backed or GpuMemoryBuffer-backed
  // resource. It is given from a child to the service, and waited on in order
  // to use the resource, and this is tracked by the |synchronization_state_|.
  gpu::SyncToken sync_token_;

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

}  // namespace internal
}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_H_
