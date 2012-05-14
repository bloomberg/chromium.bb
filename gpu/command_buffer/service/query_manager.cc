// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/query_manager.h"
#include "base/atomicops.h"
#include "base/logging.h"
#include "base/time.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/feature_info.h"

namespace gpu {
namespace gles2 {

class AllSamplesPassedQuery : public QueryManager::Query {
 public:
  AllSamplesPassedQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset,
      GLuint service_id);
  virtual ~AllSamplesPassedQuery();
  virtual bool Begin() OVERRIDE;
  virtual bool End(uint32 submit_count) OVERRIDE;
  virtual bool Process() OVERRIDE;
  virtual void Destroy(bool have_context) OVERRIDE;

 private:
  // Service side query id.
  GLuint service_id_;
};

AllSamplesPassedQuery::AllSamplesPassedQuery(
    QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset,
    GLuint service_id)
    : Query(manager, target, shm_id, shm_offset),
      service_id_(service_id) {
}

AllSamplesPassedQuery::~AllSamplesPassedQuery() {
}

void AllSamplesPassedQuery::Destroy(bool have_context) {
  if (have_context && !IsDeleted()) {
    glDeleteQueriesARB(1, &service_id_);
    MarkAsDeleted();
  }
}

bool AllSamplesPassedQuery::Begin() {
  BeginQueryHelper(target(), service_id_);
  return true;
}

bool AllSamplesPassedQuery::End(uint32 submit_count) {
  EndQueryHelper(target());
  return AddToPendingQueue(submit_count);
}

bool AllSamplesPassedQuery::Process() {
  GLuint available = 0;
  glGetQueryObjectuivARB(
      service_id_, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  if (!available) {
    return true;
  }
  GLuint result = 0;
  glGetQueryObjectuivARB(
      service_id_, GL_QUERY_RESULT_EXT, &result);

  return MarkAsCompleted(result != 0);
}

class CommandsIssuedQuery : public QueryManager::Query {
 public:
  CommandsIssuedQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset);
  virtual ~CommandsIssuedQuery();

  virtual bool Begin() OVERRIDE;
  virtual bool End(uint32 submit_count) OVERRIDE;
  virtual bool Process() OVERRIDE;
  virtual void Destroy(bool have_context) OVERRIDE;

 private:
  base::TimeTicks begin_time_;
};

CommandsIssuedQuery::CommandsIssuedQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset)
    : Query(manager, target, shm_id, shm_offset) {
}

CommandsIssuedQuery::~CommandsIssuedQuery() {
}

bool CommandsIssuedQuery::Process() {
  NOTREACHED();
  return true;
}

bool CommandsIssuedQuery::Begin() {
  begin_time_ = base::TimeTicks::HighResNow();
  return true;
}

bool CommandsIssuedQuery::End(uint32 submit_count) {
  base::TimeDelta elapsed = base::TimeTicks::HighResNow() - begin_time_;
  MarkAsPending(submit_count);
  return MarkAsCompleted(
      std::min(elapsed.InMicroseconds(), static_cast<int64>(0xFFFFFFFFL)));
}

void CommandsIssuedQuery::Destroy(bool /* have_context */) {
  if (!IsDeleted()) {
    MarkAsDeleted();
  }
}

QueryManager::QueryManager(
    CommonDecoder* decoder,
    FeatureInfo* feature_info)
    : decoder_(decoder),
      use_arb_occlusion_query2_for_occlusion_query_boolean_(
          feature_info->feature_flags(
            ).use_arb_occlusion_query2_for_occlusion_query_boolean),
      use_arb_occlusion_query_for_occlusion_query_boolean_(
          feature_info->feature_flags(
            ).use_arb_occlusion_query_for_occlusion_query_boolean),
      query_count_(0) {
  DCHECK(!(use_arb_occlusion_query_for_occlusion_query_boolean_ &&
           use_arb_occlusion_query2_for_occlusion_query_boolean_));
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
    query->Destroy(have_context);
    queries_.erase(queries_.begin());
  }
}

QueryManager::Query* QueryManager::CreateQuery(
    GLenum target, GLuint client_id, int32 shm_id, uint32 shm_offset) {
  Query::Ref query;
  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
      query = new CommandsIssuedQuery(this, target, shm_id, shm_offset);
      break;
    default: {
      GLuint service_id = 0;
      glGenQueriesARB(1, &service_id);
      DCHECK_NE(0u, service_id);
      query = new AllSamplesPassedQuery(
          this, target, shm_id, shm_offset, service_id);
      break;
    }
  }
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

GLenum QueryManager::AdjustTargetForEmulation(GLenum target) {
  switch (target) {
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT:
    case GL_ANY_SAMPLES_PASSED_EXT:
      if (use_arb_occlusion_query2_for_occlusion_query_boolean_) {
        // ARB_occlusion_query2 does not have a
        // GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT
        // target.
        target = GL_ANY_SAMPLES_PASSED_EXT;
      } else if (use_arb_occlusion_query_for_occlusion_query_boolean_) {
        // ARB_occlusion_query does not have a
        // GL_ANY_SAMPLES_PASSED_EXT
        // target.
        target = GL_SAMPLES_PASSED_ARB;
      }
      break;
    default:
      break;
  }
  return target;
}

void QueryManager::BeginQueryHelper(GLenum target, GLuint id) {
  target = AdjustTargetForEmulation(target);
  glBeginQueryARB(target, id);
}

void QueryManager::EndQueryHelper(GLenum target) {
  target = AdjustTargetForEmulation(target);
  glEndQueryARB(target);
}

QueryManager::Query::Query(
     QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset)
    : manager_(manager),
      target_(target),
      shm_id_(shm_id),
      shm_offset_(shm_offset),
      submit_count_(0),
      pending_(false),
      deleted_(false) {
  DCHECK(manager);
  manager_->StartTracking(this);
}

QueryManager::Query::~Query() {
  if (manager_) {
    manager_->StopTracking(this);
    manager_ = NULL;
  }
}

bool QueryManager::Query::MarkAsCompleted(GLuint result) {
  DCHECK(pending_);
  QuerySync* sync = manager_->decoder_->GetSharedMemoryAs<QuerySync*>(
          shm_id_, shm_offset_, sizeof(*sync));
  if (!sync) {
    return false;
  }

  pending_ = false;
  sync->result = result;
  // Need a MemoryBarrier here so that sync->result is written before
  // sync->process_count.
  base::subtle::MemoryBarrier();
  sync->process_count = submit_count_;

  return true;
}

bool QueryManager::ProcessPendingQueries() {
  while (!pending_queries_.empty()) {
    Query* query = pending_queries_.front().get();
    if (!query->Process()) {
      return false;
    }
    if (query->pending()) {
      return true;
    }
    pending_queries_.pop_front();
  }

  return true;
}

bool QueryManager::HavePendingQueries() {
  return !pending_queries_.empty();
}

bool QueryManager::AddPendingQuery(Query* query, uint32 submit_count) {
  DCHECK(query);
  DCHECK(!query->IsDeleted());
  if (!RemovePendingQuery(query)) {
    return false;
  }
  query->MarkAsPending(submit_count);
  pending_queries_.push_back(query);
  return true;
}

bool QueryManager::RemovePendingQuery(Query* query) {
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
    if (!query->MarkAsCompleted(0)) {
      return false;
    }
  }
  return true;
}

bool QueryManager::BeginQuery(Query* query) {
  DCHECK(query);
  if (!RemovePendingQuery(query)) {
    return false;
  }
  return query->Begin();
}

bool QueryManager::EndQuery(Query* query, uint32 submit_count) {
  DCHECK(query);
  if (!RemovePendingQuery(query)) {
    return false;
  }
  return query->End(submit_count);
}

}  // namespace gles2
}  // namespace gpu


