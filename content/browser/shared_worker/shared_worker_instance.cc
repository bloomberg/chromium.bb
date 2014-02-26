// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_instance.h"

#include "base/logging.h"
#include "content/browser/worker_host/worker_document_set.h"

namespace content {

SharedWorkerInstance::SharedWorkerInstance(
    const GURL& url,
    const base::string16& name,
    const base::string16& content_security_policy,
    blink::WebContentSecurityPolicyType security_policy_type,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition)
    : url_(url),
      closed_(false),
      name_(name),
      content_security_policy_(content_security_policy),
      security_policy_type_(security_policy_type),
      worker_document_set_(new WorkerDocumentSet()),
      resource_context_(resource_context),
      partition_(partition),
      load_failed_(false) {
  DCHECK(resource_context_);
}

SharedWorkerInstance::~SharedWorkerInstance() {
}

void SharedWorkerInstance::SetMessagePortID(
    SharedWorkerMessageFilter* filter,
    int route_id,
    int message_port_id) {
  for (FilterList::iterator i = filters_.begin(); i != filters_.end(); ++i) {
    if (i->filter() == filter && i->route_id() == route_id) {
      i->set_message_port_id(message_port_id);
      return;
    }
  }
}

bool SharedWorkerInstance::Matches(const GURL& match_url,
                                   const base::string16& match_name,
                                   const WorkerStoragePartition& partition,
    ResourceContext* resource_context) const {
  // Only match open shared workers.
  if (closed_)
    return false;

  // ResourceContext equivalence is being used as a proxy to ensure we only
  // matched shared workers within the same BrowserContext.
  if (resource_context_ != resource_context)
    return false;

  // We must be in the same storage partition otherwise sharing will violate
  // isolation.
  if (!partition_.Equals(partition))
    return false;

  if (url_.GetOrigin() != match_url.GetOrigin())
    return false;

  if (name_.empty() && match_name.empty())
    return url_ == match_url;

  return name_ == match_name;
}

void SharedWorkerInstance::AddFilter(SharedWorkerMessageFilter* filter,
                                     int route_id) {
  CHECK(filter);
  if (!HasFilter(filter, route_id)) {
    FilterInfo info(filter, route_id);
    filters_.push_back(info);
  }
}

void SharedWorkerInstance::RemoveFilters(SharedWorkerMessageFilter* filter) {
  for (FilterList::iterator i = filters_.begin(); i != filters_.end();) {
    if (i->filter() == filter)
      i = filters_.erase(i);
    else
      ++i;
  }
}

bool SharedWorkerInstance::HasFilter(SharedWorkerMessageFilter* filter,
                                     int route_id) const {
  for (FilterList::const_iterator i = filters_.begin(); i != filters_.end();
       ++i) {
    if (i->filter() == filter && i->route_id() == route_id)
      return true;
  }
  return false;
}

}  // namespace content
