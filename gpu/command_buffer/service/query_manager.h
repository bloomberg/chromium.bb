// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_QUERY_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_QUERY_MANAGER_H_

#include <deque>
#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_export.h"

namespace gpu {

class CommonDecoder;

namespace gles2 {

class FeatureInfo;

// This class keeps track of the queries and their state
// As Queries are not shared there is one QueryManager per context.
class GPU_EXPORT QueryManager {
 public:
  class GPU_EXPORT Query : public base::RefCounted<Query> {
   public:
    typedef scoped_refptr<Query> Ref;

    Query(
       QueryManager* manager, GLenum target, int32 shm_id, uint32 shm_offset);
    virtual ~Query();

    GLenum target() const {
      return target_;
    }

    bool IsDeleted() const {
      return deleted_;
    }

    bool IsValid() const {
      return target() && !IsDeleted();
    }

    bool pending() const {
      return pending_;
    }

    int32 shm_id() const {
      return shm_id_;
    }

    uint32 shm_offset() const {
      return shm_offset_;
    }

    // Returns false if shared memory for sync is invalid.
    virtual bool Begin() = 0;

    // Returns false if shared memory for sync is invalid.
    virtual bool End(uint32 submit_count) = 0;

    // Returns false if shared memory for sync is invalid.
    virtual bool Process() = 0;

    virtual void Destroy(bool have_context) = 0;

   protected:
    QueryManager* manager() const {
      return manager_;
    }

    void MarkAsDeleted() {
      deleted_ = true;
    }

    // Returns false if shared memory for sync is invalid.
    bool MarkAsCompleted(GLuint result);

    void MarkAsPending(uint32 submit_count) {
      DCHECK(!pending_);
      pending_ = true;
      submit_count_ = submit_count;
    }

    // Returns false if shared memory for sync is invalid.
    bool AddToPendingQueue(uint32 submit_count) {
      return manager_->AddPendingQuery(this, submit_count);
    }

    void BeginQueryHelper(GLenum target, GLuint id) {
      manager_->BeginQueryHelper(target, id);
    }

    void EndQueryHelper(GLenum target) {
      manager_->EndQueryHelper(target);
    }

   private:
    friend class QueryManager;
    friend class QueryManagerTest;
    friend class base::RefCounted<Query>;

    uint32 submit_count() const {
      return submit_count_;
    }

    // The manager that owns this Query.
    QueryManager* manager_;

    // The type of query.
    GLenum target_;

    // The shared memory used with this Query.
    int32 shm_id_;
    uint32 shm_offset_;

    // Count to set process count do when completed.
    uint32 submit_count_;

    // True if in the queue.
    bool pending_;

    // True if deleted.
    bool deleted_;
  };

  QueryManager(
      CommonDecoder* decoder,
      FeatureInfo* feature_info);
  ~QueryManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a Query for the given query.
  Query* CreateQuery(
      GLenum target, GLuint client_id, int32 shm_id, uint32 shm_offset);

  // Gets the query info for the given query.
  Query* GetQuery(GLuint client_id);

  // Removes a query info for the given query.
  void RemoveQuery(GLuint client_id);

  // Returns false if any query is pointing to invalid shared memory.
  bool BeginQuery(Query* query);

  // Returns false if any query is pointing to invalid shared memory.
  bool EndQuery(Query* query, uint32 submit_count);

  // Processes pending queries. Returns false if any queries are pointing
  // to invalid shared memory.
  bool ProcessPendingQueries();

  // True if there are pending queries.
  bool HavePendingQueries();

 private:
  void StartTracking(Query* query);
  void StopTracking(Query* query);

  // Wrappers for BeginQueryARB and EndQueryARB to hide differences between
  // ARB_occlusion_query2 and EXT_occlusion_query_boolean.
  void BeginQueryHelper(GLenum target, GLuint id);
  void EndQueryHelper(GLenum target);

  // Adds to queue of queries waiting for completion.
  // Returns false if any query is pointing to invalid shared memory.
  bool AddPendingQuery(Query* query, uint32 submit_count);

  // Removes a query from the queue of pending queries.
  // Returns false if any query is pointing to invalid shared memory.
  bool RemovePendingQuery(Query* query);

  // Returns a target used for the underlying GL extension
  // used to emulate a query.
  GLenum AdjustTargetForEmulation(GLenum target);

  // Used to validate shared memory.
  CommonDecoder* decoder_;

  bool use_arb_occlusion_query2_for_occlusion_query_boolean_;
  bool use_arb_occlusion_query_for_occlusion_query_boolean_;

  // Counts the number of Queries allocated with 'this' as their manager.
  // Allows checking no Query will outlive this.
  unsigned query_count_;

  // Info for each query in the system.
  typedef base::hash_map<GLuint, Query::Ref> QueryMap;
  QueryMap queries_;

  // Queries waiting for completion.
  typedef std::deque<Query::Ref> QueryQueue;
  QueryQueue pending_queries_;

  DISALLOW_COPY_AND_ASSIGN(QueryManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_QUERY_MANAGER_H_
