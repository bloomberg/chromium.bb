// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "../client/query_tracker.h"

#include "../client/atomicops.h"
#include "../client/gles2_cmd_helper.h"
#include "../client/gles2_implementation.h"
#include "../client/mapped_memory.h"
#include "../common/time.h"

namespace gpu {
namespace gles2 {

QuerySyncManager::QuerySyncManager(MappedMemoryManager* manager)
    : mapped_memory_(manager) {
  GPU_DCHECK(manager);
}

QuerySyncManager::~QuerySyncManager() {
  while (!buckets_.empty()) {
    mapped_memory_->Free(buckets_.front());
    buckets_.pop();
  }
}

bool QuerySyncManager::Alloc(QuerySyncManager::QueryInfo* info) {
  GPU_DCHECK(info);
  if (free_queries_.empty()) {
    int32 shm_id;
    unsigned int shm_offset;
    void* mem = mapped_memory_->Alloc(
        kSyncsPerBucket * sizeof(QuerySync), &shm_id, &shm_offset);
    if (!mem) {
      return false;
    }
    QuerySync* syncs = static_cast<QuerySync*>(mem);
    buckets_.push(syncs);
    for (size_t ii = 0; ii < kSyncsPerBucket; ++ii) {
      free_queries_.push(QueryInfo(shm_id, shm_offset, syncs));
      ++syncs;
      shm_offset += sizeof(*syncs);
    }
  }
  *info = free_queries_.front();
  info->sync->Reset();
  free_queries_.pop();
  return true;
}

void QuerySyncManager::Free(const QuerySyncManager::QueryInfo& info) {
  free_queries_.push(info);
}

QueryTracker::Query::Query(GLuint id, GLenum target,
                           const QuerySyncManager::QueryInfo& info)
    : id_(id),
      target_(target),
      info_(info),
      state_(kUninitialized),
      submit_count_(0),
      token_(0),
      flushed_(false),
      client_begin_time_us_(0),
      result_(0) {
    }


void QueryTracker::Query::Begin(GLES2Implementation* gl) {
  // init memory, inc count
  MarkAsActive();

  switch (target()) {
    case GL_GET_ERROR_QUERY_CHROMIUM:
      // To nothing on begin for error queries.
      break;
    case GL_LATENCY_QUERY_CHROMIUM:
      client_begin_time_us_ = MicrosecondsSinceOriginOfTime();
      // tell service about id, shared memory and count
      gl->helper()->BeginQueryEXT(target(), id(), shm_id(), shm_offset());
      break;
    case GL_ASYNC_PIXEL_TRANSFERS_COMPLETED_CHROMIUM:
      // tell service about id, shared memory and count
      gl->helper()->BeginQueryEXT(target(), id(), shm_id(), shm_offset());
      break;
    default:
      // tell service about id, shared memory and count
      gl->helper()->BeginQueryEXT(target(), id(), shm_id(), shm_offset());
      break;
  }
}

void QueryTracker::Query::End(GLES2Implementation* gl) {
  switch (target()) {
    case GL_GET_ERROR_QUERY_CHROMIUM: {
      GLenum error = gl->GetClientSideGLError();
      if (error == GL_NO_ERROR) {
        // There was no error so start the query on the serivce.
        // it will end immediately.
        gl->helper()->BeginQueryEXT(target(), id(), shm_id(), shm_offset());
      } else {
        // There's an error on the client, no need to bother the service. just
        // set the query as completed and return the error.
        if (error != GL_NO_ERROR) {
          state_ = kComplete;
          result_ = error;
          return;
        }
      }
    }
  }
  gl->helper()->EndQueryEXT(target(), submit_count());
  MarkAsPending(gl->helper()->InsertToken());
}

bool QueryTracker::Query::CheckResultsAvailable(
    CommandBufferHelper* helper) {
  if (Pending()) {
    if (info_.sync->process_count == submit_count_ ||
        helper->IsContextLost()) {
      // Need a MemoryBarrier here so that sync->result read after
      // sync->process_count.
      gpu::MemoryBarrier();
      switch (target()) {
        case GL_COMMANDS_ISSUED_CHROMIUM:
          result_ = std::min(info_.sync->result,
                             static_cast<uint64>(0xFFFFFFFFL));
          break;
        case GL_LATENCY_QUERY_CHROMIUM:
          GPU_DCHECK(info_.sync->result >= client_begin_time_us_);
          result_ = std::min(info_.sync->result - client_begin_time_us_,
                             static_cast<uint64>(0xFFFFFFFFL));
          break;
        case GL_ASYNC_PIXEL_TRANSFERS_COMPLETED_CHROMIUM:
          result_ = info_.sync->result;
          break;
        default:
          result_ = info_.sync->result;
          break;
      }
      state_ = kComplete;
    } else {
      if (!flushed_) {
        // TODO(gman): We could reduce the number of flushes by having a
        // flush count, recording that count at the time we insert the
        // EndQuery command and then only flushing here if we've have not
        // passed that count yet.
        flushed_ = true;
        helper->Flush();
      } else {
        // Insert no-ops so that eventually the GPU process will see more work.
        helper->Noop(1);
      }
    }
  }
  return state_ == kComplete;
}

uint32 QueryTracker::Query::GetResult() const {
  GPU_DCHECK(state_ == kComplete || state_ == kUninitialized);
  return result_;
}

QueryTracker::QueryTracker(MappedMemoryManager* manager)
    : query_sync_manager_(manager) {
}

QueryTracker::~QueryTracker() {
  queries_.clear();
}

QueryTracker::Query* QueryTracker::CreateQuery(GLuint id, GLenum target) {
  GPU_DCHECK_NE(0u, id);
  QuerySyncManager::QueryInfo info;
  if (!query_sync_manager_.Alloc(&info)) {
    return NULL;
  }
  Query* query = new Query(id, target, info);
  std::pair<QueryMap::iterator, bool> result =
      queries_.insert(std::make_pair(id, query));
  GPU_DCHECK(result.second);
  return query;
}

QueryTracker::Query* QueryTracker::GetQuery(
    GLuint client_id) {
  QueryMap::iterator it = queries_.find(client_id);
  return it != queries_.end() ? it->second : NULL;
}

void QueryTracker::RemoveQuery(GLuint client_id, bool context_lost) {
  (void)context_lost;  // stop unused warning
  QueryMap::iterator it = queries_.find(client_id);
  if (it != queries_.end()) {
    Query* query = it->second;
    GPU_DCHECK(context_lost || !query->Pending());
    query_sync_manager_.Free(query->info_);
    queries_.erase(it);
    delete query;
  }
}

}  // namespace gles2
}  // namespace gpu
