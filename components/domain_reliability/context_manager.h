// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_MANAGER_H_
#define COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_MANAGER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/domain_reliability/beacon.h"
#include "components/domain_reliability/config.h"
#include "components/domain_reliability/context.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "url/gurl.h"

namespace domain_reliability {

class DOMAIN_RELIABILITY_EXPORT DomainReliabilityContextManager {
 public:
  DomainReliabilityContextManager(
      DomainReliabilityContext::Factory* context_factory);
  ~DomainReliabilityContextManager();

  // If |url| maps to a context added to this manager, calls |OnBeacon| on
  // that context with |beacon|. Otherwise, does nothing.
  void RouteBeacon(const GURL& url, const DomainReliabilityBeacon& beacon);

  // Calls |ClearBeacons| on all contexts added to this manager, but leaves
  // the contexts themselves intact.
  void ClearBeaconsInAllContexts();

  // TODO(ttuttle): Once unit tests test ContextManager directly, they can use
  // a custom Context::Factory to get the created Context, and this can be void.
  DomainReliabilityContext* AddContextForConfig(
      scoped_ptr<const DomainReliabilityConfig> config);

  // Removes all contexts from this manager (discarding all queued beacons in
  // the process).
  void RemoveAllContexts();

  scoped_ptr<base::Value> GetWebUIData() const;

  size_t contexts_size_for_testing() const { return contexts_.size(); }

 private:
  typedef std::map<std::string, DomainReliabilityContext*> ContextMap;

  DomainReliabilityContext* GetContextForHost(const std::string& host);

  DomainReliabilityContext::Factory* context_factory_;
  // Owns DomainReliabilityContexts.
  ContextMap contexts_;

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityContextManager);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_CONTEXT_MANAGER_H_
