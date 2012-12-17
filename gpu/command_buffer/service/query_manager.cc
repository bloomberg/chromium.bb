// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/query_manager.h"
#include "base/atomicops.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/time.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "ui/gl/async_pixel_transfer_delegate.h"

namespace gpu {
namespace gles2 {

class AllSamplesPassedQuery : public QueryManager::Query {
 public:
  AllSamplesPassedQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset,
      GLuint service_id);
  virtual bool Begin() OVERRIDE;
  virtual bool End(uint32 submit_count) OVERRIDE;
  virtual bool Process() OVERRIDE;
  virtual void Destroy(bool have_context) OVERRIDE;

 protected:
  virtual ~AllSamplesPassedQuery();

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

void AllSamplesPassedQuery::Destroy(bool have_context) {
  if (have_context && !IsDeleted()) {
    glDeleteQueriesARB(1, &service_id_);
    MarkAsDeleted();
  }
}

AllSamplesPassedQuery::~AllSamplesPassedQuery() {
}

class CommandsIssuedQuery : public QueryManager::Query {
 public:
  CommandsIssuedQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset);

  virtual bool Begin() OVERRIDE;
  virtual bool End(uint32 submit_count) OVERRIDE;
  virtual bool Process() OVERRIDE;
  virtual void Destroy(bool have_context) OVERRIDE;

 protected:
  virtual ~CommandsIssuedQuery();

 private:
  base::TimeTicks begin_time_;
};

CommandsIssuedQuery::CommandsIssuedQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset)
    : Query(manager, target, shm_id, shm_offset) {
}

bool CommandsIssuedQuery::Begin() {
  begin_time_ = base::TimeTicks::HighResNow();
  return true;
}

bool CommandsIssuedQuery::End(uint32 submit_count) {
  base::TimeDelta elapsed = base::TimeTicks::HighResNow() - begin_time_;
  MarkAsPending(submit_count);
  return MarkAsCompleted(elapsed.InMicroseconds());
}

bool CommandsIssuedQuery::Process() {
  NOTREACHED();
  return true;
}

void CommandsIssuedQuery::Destroy(bool /* have_context */) {
  if (!IsDeleted()) {
    MarkAsDeleted();
  }
}

CommandsIssuedQuery::~CommandsIssuedQuery() {
}

class CommandLatencyQuery : public QueryManager::Query {
 public:
  CommandLatencyQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset);

  virtual bool Begin() OVERRIDE;
  virtual bool End(uint32 submit_count) OVERRIDE;
  virtual bool Process() OVERRIDE;
  virtual void Destroy(bool have_context) OVERRIDE;

 protected:
  virtual ~CommandLatencyQuery();
};

CommandLatencyQuery::CommandLatencyQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset)
    : Query(manager, target, shm_id, shm_offset) {
}

bool CommandLatencyQuery::Begin() {
    return true;
}

bool CommandLatencyQuery::End(uint32 submit_count) {
    base::TimeDelta now = base::TimeTicks::HighResNow() - base::TimeTicks();
    MarkAsPending(submit_count);
    return MarkAsCompleted(now.InMicroseconds());
}

bool CommandLatencyQuery::Process() {
  NOTREACHED();
  return true;
}

void CommandLatencyQuery::Destroy(bool /* have_context */) {
  if (!IsDeleted()) {
    MarkAsDeleted();
  }
}

CommandLatencyQuery::~CommandLatencyQuery() {
}

class AsyncPixelTransfersCompletedQuery
    : public QueryManager::Query
    , public base::SupportsWeakPtr<AsyncPixelTransfersCompletedQuery> {
 public:
  AsyncPixelTransfersCompletedQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset);

  virtual bool Begin() OVERRIDE;
  virtual bool End(uint32 submit_count) OVERRIDE;
  virtual bool Process() OVERRIDE;
  virtual void Destroy(bool have_context) OVERRIDE;

  void MarkAsCompletedCallback() { MarkAsCompleted(1); }

 protected:
  virtual ~AsyncPixelTransfersCompletedQuery();
};

AsyncPixelTransfersCompletedQuery::AsyncPixelTransfersCompletedQuery(
    QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset)
    : Query(manager, target, shm_id, shm_offset) {
}

bool AsyncPixelTransfersCompletedQuery::Begin() {
  return true;
}

bool AsyncPixelTransfersCompletedQuery::End(uint32 submit_count) {
  MarkAsPending(submit_count);

  // This will call MarkAsCompleted(1) as a reply to a task on
  // the async upload thread, such that it occurs after all previous
  // async transfers have completed.
  manager()->decoder()->GetAsyncPixelTransferDelegate()->AsyncNotifyCompletion(
      base::Bind(
          &AsyncPixelTransfersCompletedQuery::MarkAsCompletedCallback,
          AsWeakPtr()));

  // TODO(epenner): The async task occurs outside the normal
  // flow, via a callback on this thread. Is there anything
  // missing or wrong with that?

  // TODO(epenner): Could we possibly trigger the completion on
  // the upload thread by writing to the query shared memory
  // directly?
  return true;
}

bool AsyncPixelTransfersCompletedQuery::Process() {
  NOTREACHED();
  return true;
}

void AsyncPixelTransfersCompletedQuery::Destroy(bool /* have_context */) {
  if (!IsDeleted()) {
    MarkAsDeleted();
  }
}

AsyncPixelTransfersCompletedQuery::~AsyncPixelTransfersCompletedQuery() {
}

class GetErrorQuery : public QueryManager::Query {
 public:
  GetErrorQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset);

  virtual bool Begin() OVERRIDE;
  virtual bool End(uint32 submit_count) OVERRIDE;
  virtual bool Process() OVERRIDE;
  virtual void Destroy(bool have_context) OVERRIDE;

 protected:
  virtual ~GetErrorQuery();

 private:
};

GetErrorQuery::GetErrorQuery(
      QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset)
    : Query(manager, target, shm_id, shm_offset) {
}

bool GetErrorQuery::Begin() {
  return true;
}

bool GetErrorQuery::End(uint32 submit_count) {
  MarkAsPending(submit_count);
  return MarkAsCompleted(manager()->decoder()->GetGLError());
}

bool GetErrorQuery::Process() {
  NOTREACHED();
  return true;
}

void GetErrorQuery::Destroy(bool /* have_context */) {
  if (!IsDeleted()) {
    MarkAsDeleted();
  }
}

GetErrorQuery::~GetErrorQuery() {
}

QueryManager::QueryManager(
    GLES2Decoder* decoder,
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
    case GL_LATENCY_QUERY_CHROMIUM:
      query = new CommandLatencyQuery(this, target, shm_id, shm_offset);
      break;
    case GL_ASYNC_PIXEL_TRANSFERS_COMPLETED_CHROMIUM:
      query = new AsyncPixelTransfersCompletedQuery(
          this, target, shm_id, shm_offset);
      break;
    case GL_GET_ERROR_QUERY_CHROMIUM:
      query = new GetErrorQuery(this, target, shm_id, shm_offset);
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

bool QueryManager::Query::MarkAsCompleted(uint64 result) {
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


