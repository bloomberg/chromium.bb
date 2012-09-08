// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../client/atomicops.h"
#include "../client/share_group.h"
#include "../client/gles2_implementation.h"
#include "../client/program_info_manager.h"
#include "../common/id_allocator.h"
#include "../common/logging.h"

namespace gpu {
namespace gles2 {

COMPILE_ASSERT(gpu::kInvalidResource == 0,
               INVALID_RESOURCE_NOT_0_AS_GL_EXPECTS);

// The standard id handler.
class IdHandler : public IdHandlerInterface {
 public:
  IdHandler() { }
  virtual ~IdHandler() { }

  // Overridden from IdHandlerInterface.
  virtual void Destroy(GLES2Implementation* /* gl_impl */) OVERRIDE {
  }

  // Overridden from IdHandlerInterface.
  virtual void MakeIds(
      GLES2Implementation* /* gl_impl */,
      GLuint id_offset, GLsizei n, GLuint* ids) OVERRIDE {
    if (id_offset == 0) {
      for (GLsizei ii = 0; ii < n; ++ii) {
        ids[ii] = id_allocator_.AllocateID();
      }
    } else {
      for (GLsizei ii = 0; ii < n; ++ii) {
        ids[ii] = id_allocator_.AllocateIDAtOrAbove(id_offset);
        id_offset = ids[ii] + 1;
      }
    }
  }

  // Overridden from IdHandlerInterface.
  virtual bool FreeIds(
      GLES2Implementation* gl_impl,
      GLsizei n, const GLuint* ids, DeleteFn delete_fn) OVERRIDE {
    for (GLsizei ii = 0; ii < n; ++ii) {
      id_allocator_.FreeID(ids[ii]);
    }
    (gl_impl->*delete_fn)(n, ids);
    // We need to ensure that the delete call is evaluated on the service side
    // before any other contexts issue commands using these client ids.
    gl_impl->helper()->CommandBufferHelper::Flush();
    return true;
  }

  // Overridden from IdHandlerInterface.
  virtual bool MarkAsUsedForBind(GLuint id) OVERRIDE {
    return id == 0 ? true : id_allocator_.MarkAsUsed(id);
  }
 protected:
  IdAllocator id_allocator_;
};

// An id handler that require Gen before Bind.
class StrictIdHandler : public IdHandler {
 public:
  StrictIdHandler() { }
  virtual ~StrictIdHandler() { }

  // Overridden from IdHandler.
  virtual bool MarkAsUsedForBind(GLuint id) {
    GPU_DCHECK(id == 0 || id_allocator_.InUse(id));
    return IdHandler::MarkAsUsedForBind(id);
  }
};

// An id handler for ids that are never reused.
class NonReusedIdHandler : public IdHandlerInterface {
 public:
  NonReusedIdHandler() : last_id_(0) {}
  virtual ~NonReusedIdHandler() {}

  // Overridden from IdHandlerInterface.
  virtual void Destroy(GLES2Implementation* /* gl_impl */) OVERRIDE {
  }

  // Overridden from IdHandlerInterface.
  virtual void MakeIds(
      GLES2Implementation* /* gl_impl */,
      GLuint id_offset, GLsizei n, GLuint* ids) OVERRIDE {
    for (GLsizei ii = 0; ii < n; ++ii) {
      ids[ii] = ++last_id_ + id_offset;
    }
  }

  // Overridden from IdHandlerInterface.
  virtual bool FreeIds(
      GLES2Implementation* gl_impl,
      GLsizei n, const GLuint* ids, DeleteFn delete_fn) OVERRIDE {
    // Ids are never freed.
    (gl_impl->*delete_fn)(n, ids);
    return true;
  }

  // Overridden from IdHandlerInterface.
  virtual bool MarkAsUsedForBind(GLuint /* id */) OVERRIDE {
    // This is only used for Shaders and Programs which have no bind.
    return false;
  }

 private:
  GLuint last_id_;
};

// An id handler for shared ids.
class SharedIdHandler : public IdHandlerInterface {
 public:
  SharedIdHandler(
      id_namespaces::IdNamespaces id_namespace)
      : id_namespace_(id_namespace) {
  }

  virtual ~SharedIdHandler() { }

  // Overridden from IdHandlerInterface.
  virtual void Destroy(GLES2Implementation* /* gl_impl */) {
  }

  virtual void MakeIds(
      GLES2Implementation* gl_impl,
      GLuint id_offset, GLsizei n, GLuint* ids) {
    gl_impl->GenSharedIdsCHROMIUM(id_namespace_, id_offset, n, ids);
  }

  virtual bool FreeIds(
      GLES2Implementation* gl_impl,
      GLsizei n, const GLuint* ids, DeleteFn delete_fn) {
    gl_impl->DeleteSharedIdsCHROMIUM(id_namespace_, n, ids);
    (gl_impl->*delete_fn)(n, ids);
    // We need to ensure that the delete call is evaluated on the service side
    // before any other contexts issue commands using these client ids.
    gl_impl->helper()->CommandBufferHelper::Flush();
    return true;
  }

  virtual bool MarkAsUsedForBind(GLuint id) {
    // This has no meaning for shared resources.
    return true;
  }

 private:
  id_namespaces::IdNamespaces id_namespace_;
};

class ThreadSafeIdHandlerWrapper : public IdHandlerInterface {
 public:
  ThreadSafeIdHandlerWrapper(IdHandlerInterface* id_handler)
      : id_handler_(id_handler) {
  }
  virtual ~ThreadSafeIdHandlerWrapper() { }

  // Overridden from IdHandlerInterface.
  virtual void Destroy(GLES2Implementation* gl_impl) {
    AutoLock auto_lock(lock_);
    id_handler_->Destroy(gl_impl);
  }

  // Overridden from IdHandlerInterface.
  virtual void MakeIds(
      GLES2Implementation* gl_impl, GLuint id_offset, GLsizei n, GLuint* ids) {
    AutoLock auto_lock(lock_);
    id_handler_->MakeIds(gl_impl, id_offset, n, ids);
  }

  // Overridden from IdHandlerInterface.
  virtual bool FreeIds(
      GLES2Implementation* gl_impl, GLsizei n, const GLuint* ids,
      DeleteFn delete_fn) {
    AutoLock auto_lock(lock_);
    return id_handler_->FreeIds(gl_impl, n, ids, delete_fn);
  }

  // Overridden from IdHandlerInterface.
  virtual bool MarkAsUsedForBind(GLuint id) {
    AutoLock auto_lock(lock_);
    return id_handler_->MarkAsUsedForBind(id);
  }

 private:
   IdHandlerInterface* id_handler_;
   Lock lock_;
};

ShareGroup::ShareGroup(bool share_resources, bool bind_generates_resource)
    : sharing_resources_(share_resources),
      bind_generates_resource_(bind_generates_resource),
      gles2_(NULL) {
  if (bind_generates_resource) {
    for (int i = 0; i < id_namespaces::kNumIdNamespaces; ++i) {
      if (i == id_namespaces::kProgramsAndShaders) {
        id_handlers_[i].reset(new ThreadSafeIdHandlerWrapper(
            new NonReusedIdHandler()));
      } else {
        id_handlers_[i].reset(new ThreadSafeIdHandlerWrapper(
            new IdHandler()));
      }
    }
  } else {
    for (int i = 0; i < id_namespaces::kNumIdNamespaces; ++i) {
      if (i == id_namespaces::kProgramsAndShaders) {
        id_handlers_[i].reset(new ThreadSafeIdHandlerWrapper(
            new NonReusedIdHandler()));
      } else {
        id_handlers_[i].reset(new ThreadSafeIdHandlerWrapper(
            new StrictIdHandler()));
      }
    }
  }
  program_info_manager_.reset(ProgramInfoManager::Create(false));
}

void ShareGroup::SetGLES2ImplementationForDestruction(
    GLES2Implementation* gl_impl) {
  gles2_ = gl_impl;
}

void ShareGroup::set_program_info_manager(ProgramInfoManager* manager) {
  program_info_manager_.reset(manager);
}

ShareGroup::~ShareGroup() {
  for (int i = 0; i < id_namespaces::kNumIdNamespaces; ++i) {
    id_handlers_[i]->Destroy(gles2_);
    id_handlers_[i].reset();
  }
}

}  // namespace gles2
}  // namespace gpu
