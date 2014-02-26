// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_INSTANCE_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_INSTANCE_H_

#include <list>
#include <string>

#include "base/basictypes.h"
#include "content/browser/worker_host/worker_document_set.h"
#include "content/browser/worker_host/worker_storage_partition.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebContentSecurityPolicy.h"
#include "url/gurl.h"

namespace content {
class ResourceContext;
class SharedWorkerMessageFilter;

class CONTENT_EXPORT SharedWorkerInstance {
 public:
  // Unique identifier for a worker client.
  class FilterInfo {
   public:
    FilterInfo(SharedWorkerMessageFilter* filter, int route_id)
        : filter_(filter), route_id_(route_id), message_port_id_(0) { }
    SharedWorkerMessageFilter* filter() const { return filter_; }
    int route_id() const { return route_id_; }
    int message_port_id() const { return message_port_id_; }
    void set_message_port_id(int id) { message_port_id_ = id; }

   private:
    SharedWorkerMessageFilter* filter_;
    int route_id_;
    int message_port_id_;
  };

  typedef std::list<FilterInfo> FilterList;

  SharedWorkerInstance(const GURL& url,
                       const base::string16& name,
                       const base::string16& content_security_policy,
                       blink::WebContentSecurityPolicyType security_policy_type,
                       ResourceContext* resource_context,
                       const WorkerStoragePartition& partition);
  ~SharedWorkerInstance();

  void AddFilter(SharedWorkerMessageFilter* filter, int route_id);
  void RemoveFilters(SharedWorkerMessageFilter* filter);
  bool HasFilter(SharedWorkerMessageFilter* filter, int route_id) const;
  void SetMessagePortID(SharedWorkerMessageFilter* filter,
                        int route_id,
                        int message_port_id);
  const FilterList& filters() const { return filters_; }

  // Checks if this SharedWorkerInstance matches the passed url/name params
  // based on the algorithm in the WebWorkers spec - an instance matches if the
  // origins of the URLs match, and:
  // a) the names are non-empty and equal.
  // -or-
  // b) the names are both empty, and the urls are equal.
  bool Matches(
      const GURL& url,
      const base::string16& name,
      const WorkerStoragePartition& partition,
      ResourceContext* resource_context) const;

  // Accessors.
  bool closed() const { return closed_; }
  void set_closed(bool closed) { closed_ = closed; }
  const GURL& url() const { return url_; }
  const base::string16 name() const { return name_; }
  const base::string16 content_security_policy() const {
    return content_security_policy_;
  }
  blink::WebContentSecurityPolicyType security_policy_type() const {
    return security_policy_type_;
  }
  WorkerDocumentSet* worker_document_set() const {
    return worker_document_set_.get();
  }
  ResourceContext* resource_context() const {
    return resource_context_;
  }
  const WorkerStoragePartition& partition() const {
    return partition_;
  }
  void set_load_failed(bool failed) { load_failed_ = failed; }
  bool load_failed() { return load_failed_; }

 private:
  GURL url_;
  bool closed_;
  base::string16 name_;
  base::string16 content_security_policy_;
  blink::WebContentSecurityPolicyType security_policy_type_;
  FilterList filters_;
  scoped_refptr<WorkerDocumentSet> worker_document_set_;
  ResourceContext* const resource_context_;
  WorkerStoragePartition partition_;
  bool load_failed_;
};

}  // namespace content


#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_INSTANCE_H_
