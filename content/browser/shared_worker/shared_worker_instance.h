// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_INSTANCE_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_INSTANCE_H_

#include <string>

#include "base/unguessable_token.h"
#include "content/browser/shared_worker/worker_storage_partition.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebAddressSpace.h"
#include "third_party/WebKit/public/platform/WebContentSecurityPolicy.h"
#include "third_party/WebKit/public/web/shared_worker_creation_context_type.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class ResourceContext;

// SharedWorkerInstance is copyable value-type data type. It could be passed to
// the UI thread and be used for comparison in SharedWorkerDevToolsManager.
class CONTENT_EXPORT SharedWorkerInstance {
 public:
  SharedWorkerInstance(
      const GURL& url,
      const std::string& name,
      const url::Origin& constructor_origin,
      const std::string& content_security_policy,
      blink::WebContentSecurityPolicyType content_security_policy_type,
      blink::WebAddressSpace creation_address_space,
      ResourceContext* resource_context,
      const WorkerStoragePartitionId& partition_id,
      blink::mojom::SharedWorkerCreationContextType creation_context_type,
      const base::UnguessableToken& devtools_worker_token);
  SharedWorkerInstance(const SharedWorkerInstance& other);
  ~SharedWorkerInstance();

  // Checks if this SharedWorkerInstance matches the passed url, name, and
  // constructor origin params according to the SharedWorker constructor steps
  // in the HTML spec:
  // https://html.spec.whatwg.org/multipage/workers.html#shared-workers-and-the-sharedworker-interface
  bool Matches(const GURL& url,
               const std::string& name,
               const url::Origin& constructor_origin,
               const WorkerStoragePartitionId& partition,
               ResourceContext* resource_context) const;
  bool Matches(const SharedWorkerInstance& other) const;

  // Accessors.
  const GURL& url() const { return url_; }
  const std::string name() const { return name_; }
  const url::Origin& constructor_origin() const { return constructor_origin_; }
  const std::string content_security_policy() const {
    return content_security_policy_;
  }
  blink::WebContentSecurityPolicyType content_security_policy_type() const {
    return content_security_policy_type_;
  }
  blink::WebAddressSpace creation_address_space() const {
    return creation_address_space_;
  }
  ResourceContext* resource_context() const {
    return resource_context_;
  }
  const WorkerStoragePartitionId& partition_id() const { return partition_id_; }
  blink::mojom::SharedWorkerCreationContextType creation_context_type() const {
    return creation_context_type_;
  }
  const base::UnguessableToken& devtools_worker_token() const {
    return devtools_worker_token_;
  }

 private:
  const GURL url_;
  const std::string name_;

  // The origin of the document that created this shared worker instance. Used
  // for security checks. See Matches() for details.
  // https://html.spec.whatwg.org/multipage/workers.html#concept-sharedworkerglobalscope-constructor-origin
  const url::Origin constructor_origin_;

  const std::string content_security_policy_;
  const blink::WebContentSecurityPolicyType content_security_policy_type_;
  const blink::WebAddressSpace creation_address_space_;
  ResourceContext* const resource_context_;
  const WorkerStoragePartitionId partition_id_;
  const blink::mojom::SharedWorkerCreationContextType creation_context_type_;
  const base::UnguessableToken devtools_worker_token_;
};

}  // namespace content


#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_INSTANCE_H_
