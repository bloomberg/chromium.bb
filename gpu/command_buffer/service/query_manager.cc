// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/query_manager.h"
#include "base/atomicops.h"
#include "base/logging.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/common_decoder.h"

namespace gpu {
namespace gles2 {

QueryManager::QueryManager()
    : query_count_(0) {
}

QueryManager::~QueryManager() {
  DCHECK(queries_.empty());

  // If this triggers, that means something is keeping a reference to
  // a Query belonging to this.
  CHECK_EQ(query_count_, 0u);
}

void QueryManager::Destroy(bool have_context) {
  pending_queries_.clear();
  while (!queries_.empty()) {
    Query* query = queries_.begin()->second;
    if (have_context) {
      if (!query->IsDeleted()) {
        GLuint service_id = query->service_id();
        glDeleteQueriesARB(1, &service_id);
        query->MarkAsDeleted();
      }
    }
    queries_.erase(queries_.begin());
  }
}

QueryManager::Query* QueryManager::CreateQuery(
    GLuint client_id,
    GLuint service_id) {
  Query::Ref query(new Query(this, service_id));
  std::pair<QueryMap::iterator, bool> result =
      queries_.insert(std::make_pair(client_id, query));
  DCHECK(result.second);
  return query.get();
}

QueryManager::Query* QueryManager::GetQuery(
    GLuint client_id) {
  QueryMap::iterator it = queries_.find(client_id);
  return it != queries_.end() ? it->second : NULL;
}

void QueryManager::RemoveQuery(GLuint client_id) {
  QueryMap::iterator it = queries_.find(client_id);
  if (it != queries_.end()) {
    Query* query = it->second;
    RemovePendingQuery(query);
    query->MarkAsDeleted();
    queries_.erase(it);
  }
}

void QueryManager::StartTracking(QueryManager::Query* /* query */) {
  ++query_count_;
}

void QueryManager::StopTracking(QueryManager::Query* /* query */) {
  --query_count_;
}

QueryManager::Query::Query(
     QueryManager* manager,
     GLuint service_id)
    : manager_(manager),
      service_id_(service_id),
      target_(0),
      shm_id_(0),
      shm_offset_(0),
      pending_(false) {
  DCHECK(manager);
  manager_->StartTracking(this);
}

QueryManager::Query::~Query() {
  if (manager_) {
    manager_->StopTracking(this);
    manager_ = NULL;
  }
}

void QueryManager::Query::Initialize(
    GLenum target, int32 shm_id, uint32 shm_offset) {
  DCHECK(!IsInitialized());
  target_ = target;
  shm_id_ = shm_id;
  shm_offset_ = shm_offset;
}

bool QueryManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  DCHECK(client_id);
  // This doesn't need to be fast. It's only used during slow queries.
  for (QueryMap::const_iterator it = queries_.begin();
       it != queries_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

bool QueryManager::ProcessPendingQueries(CommonDecoder* decoder) {
  DCHECK(decoder);
  while (!pending_queries_.empty()) {
    Query* query = pending_queries_.front().get();
    GLuint available = 0;
    glGetQueryObjectuivARB(
        query->service_id(), GL_QUERY_RESULT_AVAILABLE_EXT, &available);
    if (!available) {
      return true;
    }
    GLuint result = 0;
    glGetQueryObjectuivARB(
        query->service_id(), GL_QUERY_RESULT_EXT, &result);
    QuerySync* sync = decoder->GetSharedMemoryAs<QuerySync*>(
            query->shm_id(), query->shm_offset(), sizeof(*sync));
    if (!sync) {
      return false;
    }

    sync->result = result;
    // Need a MemoryBarrier here so that sync->result is written before
    // sync->process_count.
    base::subtle::MemoryBarrier();
    sync->process_count = query->submit_count();

    query->MarkAsCompleted();
    pending_queries_.pop_front();
  }

  return true;
}

bool QueryManager::HavePendingQueries() {
  return !pending_queries_.empty();
}

void QueryManager::AddPendingQuery(Query* query, uint32 submit_count) {
  DCHECK(query);
  DCHECK(query->IsInitialized());
  DCHECK(!query->IsDeleted());
  RemovePendingQuery(query);
  query->MarkAsPending(submit_count);
  pending_queries_.push_back(query);
}

void QueryManager::RemovePendingQuery(Query* query) {
  DCHECK(query);
  if (query->pending()) {
    // TODO(gman): Speed this up if this is a common operation. This would only
    // happen if you do being/end begin/end on the same query without waiting
    // for the first one to finish.
    for (QueryQueue::iterator it = pending_queries_.begin();
         it != pending_queries_.end(); ++it) {
      if (it->get() == query) {
        pending_queries_.erase(it);
        break;
      }
    }
    query->MarkAsCompleted();
  }
}

}  // namespace gles2
}  // namespace gpu


