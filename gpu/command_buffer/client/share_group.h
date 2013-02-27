// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_SHARE_GROUP_H_
#define GPU_COMMAND_BUFFER_CLIENT_SHARE_GROUP_H_

#include <GLES2/gl2.h>
#include "../client/ref_counted.h"
#include "../common/gles2_cmd_format.h"
#include "../common/scoped_ptr.h"
#include "gles2_impl_export.h"

namespace gpu {
namespace gles2 {

class GLES2Implementation;
class GLES2ImplementationTest;
class ProgramInfoManager;

typedef void (GLES2Implementation::*DeleteFn)(GLsizei n, const GLuint* ids);

// Base class for IdHandlers
class IdHandlerInterface {
 public:
  IdHandlerInterface() { }
  virtual ~IdHandlerInterface() { }

  // Free everything.
  virtual void Destroy(GLES2Implementation* gl_impl) = 0;

  // Makes some ids at or above id_offset.
  virtual void MakeIds(
      GLES2Implementation* gl_impl,
      GLuint id_offset, GLsizei n, GLuint* ids) = 0;

  // Frees some ids.
  virtual bool FreeIds(
      GLES2Implementation* gl_impl, GLsizei n, const GLuint* ids,
      DeleteFn delete_fn) = 0;

  // Marks an id as used for glBind functions. id = 0 does nothing.
  virtual bool MarkAsUsedForBind(GLuint id) = 0;
};

// ShareGroup manages shared resources for contexts that are sharing resources.
class GLES2_IMPL_EXPORT ShareGroup
    : public gpu::RefCountedThreadSafe<ShareGroup> {
 public:
  ShareGroup(bool share_resources, bool bind_generates_resource);

  void SetGLES2ImplementationForDestruction(GLES2Implementation* gl_impl);

  bool sharing_resources() const {
    return sharing_resources_;
  }

  bool bind_generates_resource() const {
    return bind_generates_resource_;
  }

  bool Initialize();

  IdHandlerInterface* GetIdHandler(int namespace_id) const {
    return id_handlers_[namespace_id].get();
  }

  ProgramInfoManager* program_info_manager() {
    return program_info_manager_.get();
  }

 private:
  friend class gpu::RefCountedThreadSafe<ShareGroup>;
  friend class gpu::gles2::GLES2ImplementationTest;
  ~ShareGroup();

  // Install a new program info manager. Used for testing only;
  void set_program_info_manager(ProgramInfoManager* manager);

  scoped_ptr<IdHandlerInterface> id_handlers_[id_namespaces::kNumIdNamespaces];
  scoped_ptr<ProgramInfoManager> program_info_manager_;

  // Whether or not this context is sharing resources.
  bool sharing_resources_;
  bool bind_generates_resource_;

  GLES2Implementation* gles2_;

  DISALLOW_COPY_AND_ASSIGN(ShareGroup);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_SHARE_GROUP_H_
