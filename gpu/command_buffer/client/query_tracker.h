// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_QUERY_TRACKER_H_
#define GPU_COMMAND_BUFFER_CLIENT_QUERY_TRACKER_H_

#include <GLES2/gl2.h>

#include <queue>
#include "../../gpu_export.h"
#include "../client/hash_tables.h"
#include "../common/gles2_cmd_format.h"

namespace gpu {

class CommandBufferHelper;
class MappedMemoryManager;

namespace gles2 {

// Manages buckets of QuerySync instances in mapped memory.
class GPU_EXPORT QuerySyncManager {
 public:
  static const size_t kSyncsPerBucket = 4096;

  struct QueryInfo {
    QueryInfo(int32 id, uint32 offset, QuerySync* sync_mem)
        : shm_id(id),
          shm_offset(offset),
          sync(sync_mem) {
    }

    QueryInfo()
        : shm_id(0),
          shm_offset(0),
          sync(NULL) {
    }

    int32 shm_id;
    uint32 shm_offset;
    QuerySync* sync;
  };

  explicit QuerySyncManager(MappedMemoryManager* manager);
  ~QuerySyncManager();

  bool Alloc(QueryInfo* info);
  void Free(const QueryInfo& sync);

 private:
  MappedMemoryManager* mapped_memory_;
  std::queue<QuerySync*> buckets_;
  std::queue<QueryInfo> free_queries_;

  DISALLOW_COPY_AND_ASSIGN(QuerySyncManager);
};

// Tracks queries for client side of command buffer.
class GPU_EXPORT QueryTracker {
 public:
  class GPU_EXPORT Query {
   public:
    enum State {
      kUninitialized,  // never used
      kActive,         // between begin - end
      kPending,        // not yet complete
      kComplete        // completed
    };

    Query(GLuint id, GLenum target, const QuerySyncManager::QueryInfo& info)
        : id_(id),
          target_(target),
          info_(info),
          state_(kUninitialized),
          submit_count_(0),
          token_(0),
          flushed_(false),
          result_(0) {
    }

    GLenum target() const {
      return target_;
    }

    GLenum id() const {
      return id_;
    }

    int32 shm_id() const {
      return info_.shm_id;
    }

    uint32 shm_offset() const {
      return info_.shm_offset;
    }

    void MarkAsActive() {
      state_ = kActive;
      ++submit_count_;
    }

    void MarkAsPending(int32 token) {
      token_ = token;
      state_ = kPending;
      flushed_ = false;
    }

    uint32 submit_count() const {
      return submit_count_;
    }

    int32 token() const {
      return token_;
    }

    bool NeverUsed() const {
      return state_ == kUninitialized;
    }

    bool Pending() const {
      return state_ == kPending;
    }

    bool CheckResultsAvailable(CommandBufferHelper* helper);

    uint32 GetResult() const;

   private:
    friend class QueryTracker;
    friend class QueryTrackerTest;

    GLuint id_;
    GLenum target_;
    QuerySyncManager::QueryInfo info_;
    State state_;
    uint32 submit_count_;
    int32 token_;
    bool flushed_;
    uint32 result_;
  };

  QueryTracker(MappedMemoryManager* manager);
  ~QueryTracker();

  Query* CreateQuery(GLuint id, GLenum target);
  Query* GetQuery(GLuint id);
  void RemoveQuery(GLuint id);

 private:
  typedef gpu::hash_map<GLuint, Query*> QueryMap;

  QueryMap queries_;
  QuerySyncManager query_sync_manager_;

  DISALLOW_COPY_AND_ASSIGN(QueryTracker);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_QUERY_TRACKER_H_
