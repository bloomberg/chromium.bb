// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_instance.h"

#include "base/logging.h"

namespace content {

SharedWorkerInstance::SharedWorkerInstance(
    const GURL& url,
    const std::string& name,
    const url::Origin& constructor_origin,
    const std::string& content_security_policy,
    blink::WebContentSecurityPolicyType security_policy_type,
    blink::WebAddressSpace creation_address_space,
    ResourceContext* resource_context,
    const WorkerStoragePartitionId& partition_id,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    const base::UnguessableToken& devtools_worker_token)
    : url_(url),
      name_(name),
      constructor_origin_(constructor_origin),
      content_security_policy_(content_security_policy),
      content_security_policy_type_(security_policy_type),
      creation_address_space_(creation_address_space),
      resource_context_(resource_context),
      partition_id_(partition_id),
      creation_context_type_(creation_context_type),
      devtools_worker_token_(devtools_worker_token) {
  DCHECK(resource_context_);
  DCHECK(!devtools_worker_token_.is_empty());
}

SharedWorkerInstance::SharedWorkerInstance(const SharedWorkerInstance& other) =
    default;

SharedWorkerInstance::~SharedWorkerInstance() = default;

bool SharedWorkerInstance::Matches(const GURL& url,
                                   const std::string& name,
                                   const url::Origin& constructor_origin,
                                   const WorkerStoragePartitionId& partition_id,
                                   ResourceContext* resource_context) const {
  // |url| and |constructor_origin| should be in the same origin, or |url|
  // should be a data: URL.
  DCHECK(url::Origin::Create(url).IsSameOriginWith(constructor_origin) ||
         url.SchemeIs(url::kDataScheme));

  // ResourceContext equivalence is being used as a proxy to ensure we only
  // matched shared workers within the same BrowserContext.
  if (resource_context_ != resource_context)
    return false;

  // We must be in the same storage partition otherwise sharing will violate
  // isolation.
  if (!partition_id_.Equals(partition_id))
    return false;

  // Step 11.2: "If there exists a SharedWorkerGlobalScope object whose closing
  // flag is false, constructor origin is same origin with outside settings's
  // origin, constructor url equals urlRecord, and name equals the value of
  // options's name member, then set worker global scope to that
  // SharedWorkerGlobalScope object."
  if (!constructor_origin_.IsSameOriginWith(constructor_origin) ||
      url_ != url || name_ != name)
    return false;
  return true;
}

bool SharedWorkerInstance::Matches(const SharedWorkerInstance& other) const {
  return Matches(other.url(), other.name(), other.constructor_origin(),
                 other.partition_id(), other.resource_context());
}

}  // namespace content
