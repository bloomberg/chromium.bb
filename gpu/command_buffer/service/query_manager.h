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
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_export.h"

namespace gpu {

class CommonDecoder;

namespace gles2 {

// This class keeps track of the queries and their state
// As Queries are not shared there is one QueryManager per context.
class GPU_EXPORT QueryManager {
 public:
  class GPU_EXPORT Query : public base::RefCounted<Query> {
   public:
    typedef scoped_refptr<Query> Ref;

    Query(QueryManager* manager, GLuint service_id);

    void Initialize(GLenum target, int32 shm_id, uint32 shm_offset);

    bool IsInitialized() const {
      return target_ != 0;
    }

    GLuint service_id() const {
      return service_id_;
    }

    GLenum target() const {
      return target_;
    }

    bool IsDeleted() const {
      return service_id_ == 0;
    }

    bool IsValid() const {
      return target() && !IsDeleted();
    }

    int32 shm_id() const {
      return shm_id_;
    }

    uint32 shm_offset() const {
      return shm_offset_;
    }

    bool pending() const {
      return pending_;
    }

   private:
    friend class QueryManager;
    friend class QueryManagerTest;
    friend class base::RefCounted<Query>;

    ~Query();

    void MarkAsPending(uint32 submit_count) {
      DCHECK(!pending_);
      pending_ = true;
      submit_count_ = submit_count;
    }

    void MarkAsCompleted() {
      DCHECK(pending_);
      pending_ = false;
    }

    uint32 submit_count() const {
      return submit_count_;
    }

    void MarkAsDeleted() {
      service_id_ = 0;
    }

    // The manager that owns this Query.
    QueryManager* manager_;

    // Service side query id.
    GLuint service_id_;

    // The type of query.
    GLenum target_;

    // The shared memory used with this Query.
    int32 shm_id_;
    uint32 shm_offset_;

    // Count to set process count do when completed.
    uint32 submit_count_;

    // True if in the Queue.
    bool pending_;
  };

  QueryManager();
  ~QueryManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a Query for the given query.
  Query* CreateQuery(GLuint client_id, GLuint service_id);

  // Gets the query info for the given query.
  Query* GetQuery(GLuint client_id);

  // Removes a query info for the given query.
  void RemoveQuery(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

  // Adds to queue of queries waiting for completion.
  void AddPendingQuery(Query* query, uint32 submit_count);

  // Removes a query from the queue of pending queries.
  void RemovePendingQuery(Query* query);

  // Processes pending queries. Returns false if any queries are pointing
  // to invalid shared memory.
  bool ProcessPendingQueries(CommonDecoder* decoder);

  // True if there are pending queries.
  bool HavePendingQueries();

 private:
  void StartTracking(Query* query);
  void StopTracking(Query* query);

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
