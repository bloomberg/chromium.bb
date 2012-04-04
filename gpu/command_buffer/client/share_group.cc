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

// An id handler for non-shared ids.
class NonSharedIdHandler : public IdHandlerInterface {
 public:
  NonSharedIdHandler() { }
  virtual ~NonSharedIdHandler() { }

  // Overridden from IdHandlerInterface.
  virtual void Destroy(GLES2Implementation* /* gl_impl */) {
  }

  // Overridden from IdHandlerInterface.
  virtual void MakeIds(
      GLES2Implementation* /* gl_impl */,
      GLuint id_offset, GLsizei n, GLuint* ids) {
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
      GLES2Implementation* /* gl_impl */,
      GLsizei n, const GLuint* ids) {
    for (GLsizei ii = 0; ii < n; ++ii) {
      id_allocator_.FreeID(ids[ii]);
    }
    return true;
  }

  // Overridden from IdHandlerInterface.
  virtual bool MarkAsUsedForBind(GLuint id) {
    return id == 0 ? true : id_allocator_.MarkAsUsed(id);
  }
 private:
  IdAllocator id_allocator_;
};

// An id handler for non-shared ids that are never reused.
class NonSharedNonReusedIdHandler : public IdHandlerInterface {
 public:
  NonSharedNonReusedIdHandler() : last_id_(0) { }
  virtual ~NonSharedNonReusedIdHandler() { }

  // Overridden from IdHandlerInterface.
  virtual void Destroy(GLES2Implementation* /* gl_impl */) {
  }

  // Overridden from IdHandlerInterface.
  virtual void MakeIds(
      GLES2Implementation* /* gl_impl */,
      GLuint id_offset, GLsizei n, GLuint* ids) {
    for (GLsizei ii = 0; ii < n; ++ii) {
      ids[ii] = ++last_id_ + id_offset;
    }
  }

  // Overridden from IdHandlerInterface.
  virtual bool FreeIds(
      GLES2Implementation* /* gl_impl */,
      GLsizei /* n */, const GLuint* /* ids */) {
    // Ids are never freed.
    return true;
  }

  // Overridden from IdHandlerInterface.
  virtual bool MarkAsUsedForBind(GLuint /* id */) {
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
      GLsizei n, const GLuint* ids) {
    gl_impl->DeleteSharedIdsCHROMIUM(id_namespace_, n, ids);
    // We need to ensure that the delete call is evaluated on the service side
    // before any other contexts issue commands using these client ids.
    gl_impl->helper()->CommandBufferHelper::Flush();
    return true;
  }

  virtual bool MarkAsUsedForBind(GLuint /* id */) {
    // This has no meaning for shared resources.
    return true;
  }

 private:
  id_namespaces::IdNamespaces id_namespace_;
};

// An id handler for shared ids that requires ids are made before using and
// that only the context that created the id can delete it.
// Assumes the service will enforce that non made ids generate an error.
class StrictSharedIdHandler : public IdHandlerInterface {
 public:
  StrictSharedIdHandler(
      id_namespaces::IdNamespaces id_namespace)
      : id_namespace_(id_namespace) {
  }

  virtual ~StrictSharedIdHandler() {
  }

  // Overridden from IdHandlerInterface.
  virtual void Destroy(GLES2Implementation* gl_impl) {
    GPU_DCHECK(gl_impl);
    // Free all the ids not being used.
    while (!free_ids_.empty()) {
      GLuint ids[kNumIdsToGet];
      int count = 0;
      while (count < kNumIdsToGet && !free_ids_.empty()) {
        ids[count++] = free_ids_.front();
        free_ids_.pop();
      }
      gl_impl->DeleteSharedIdsCHROMIUM(id_namespace_, count, ids);
    }
  }

  virtual void MakeIds(
      GLES2Implementation* gl_impl,
      GLuint id_offset, GLsizei n, GLuint* ids) {
    GPU_DCHECK(gl_impl);
    for (GLsizei ii = 0; ii < n; ++ii) {
      ids[ii] = GetId(gl_impl, id_offset);
    }
  }

  virtual bool FreeIds(
      GLES2Implementation* /* gl_impl */,
      GLsizei n, const GLuint* ids) {
    // OpenGL sematics. If any id is bad none of them get freed.
    for (GLsizei ii = 0; ii < n; ++ii) {
      GLuint id = ids[ii];
      if (id != 0) {
        ResourceIdSet::iterator it = used_ids_.find(id);
        if (it == used_ids_.end()) {
          return false;
        }
      }
    }
    for (GLsizei ii = 0; ii < n; ++ii) {
      GLuint id = ids[ii];
      if (id != 0) {
        ResourceIdSet::iterator it = used_ids_.find(id);
        if (it != used_ids_.end()) {
          used_ids_.erase(it);
          free_ids_.push(id);
        }
      }
    }
    return true;
  }

  virtual bool MarkAsUsedForBind(GLuint id) {
    // This has no meaning for shared resources.
    return true;
  }

 private:
  static const GLsizei kNumIdsToGet = 2048;
  typedef std::queue<GLuint> ResourceIdQueue;
  typedef std::set<GLuint> ResourceIdSet;

  GLuint GetId(GLES2Implementation* gl_impl, GLuint id_offset) {
    GPU_DCHECK(gl_impl);
    if (free_ids_.empty()) {
      GLuint ids[kNumIdsToGet];
      gl_impl->GenSharedIdsCHROMIUM(
          id_namespace_, id_offset, kNumIdsToGet, ids);
      for (GLsizei ii = 0; ii < kNumIdsToGet; ++ii) {
        free_ids_.push(ids[ii]);
      }
    }
    GLuint id = free_ids_.front();
    free_ids_.pop();
    used_ids_.insert(id);
    return id;
  }

  id_namespaces::IdNamespaces id_namespace_;
  ResourceIdSet used_ids_;
  ResourceIdQueue free_ids_;
};

#ifndef _MSC_VER
const GLsizei StrictSharedIdHandler::kNumIdsToGet;
#endif

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
      GLES2Implementation* gl_impl, GLsizei n, const GLuint* ids) {
    AutoLock auto_lock(lock_);
    return id_handler_->FreeIds(gl_impl, n, ids);
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
  GPU_CHECK(ShareGroup::ImplementsThreadSafeReferenceCounting());

  if (sharing_resources_) {
    if (!bind_generates_resource_) {
      for (int i = 0; i < id_namespaces::kNumIdNamespaces; ++i) {
        id_handlers_[i].reset(new ThreadSafeIdHandlerWrapper(
            new StrictSharedIdHandler(
                static_cast<id_namespaces::IdNamespaces>(i))));
      }
    } else {
      for (int i = 0; i < id_namespaces::kNumIdNamespaces; ++i) {
        id_handlers_[i].reset(new ThreadSafeIdHandlerWrapper(
            new SharedIdHandler(
                static_cast<id_namespaces::IdNamespaces>(i))));
      }
    }
  } else {
    for (int i = 0; i < id_namespaces::kNumIdNamespaces; ++i) {
      if (i == id_namespaces::kProgramsAndShaders)
        id_handlers_[i].reset(new NonSharedNonReusedIdHandler);
      else
        id_handlers_[i].reset(new NonSharedIdHandler);
    }
  }
  program_info_manager_.reset(ProgramInfoManager::Create(sharing_resources_));
}

ShareGroup::~ShareGroup() {
  for (int i = 0; i < id_namespaces::kNumIdNamespaces; ++i) {
    id_handlers_[i]->Destroy(gles2_);
    id_handlers_[i].reset();
  }
}

void ShareGroup::SetGLES2ImplementationForDestruction(
    GLES2Implementation* gl_impl) {
  gles2_ = gl_impl;
}


}  // namespace gles2
}  // namespace gpu

