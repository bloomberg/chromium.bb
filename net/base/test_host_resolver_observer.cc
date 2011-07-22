// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_host_resolver_observer.h"

namespace net {

bool operator==(const HostResolver::RequestInfo& a,
                const HostResolver::RequestInfo& b) {
   return a.hostname() == b.hostname() &&
          a.port() == b.port() &&
          a.allow_cached_response() == b.allow_cached_response() &&
          a.priority() == b.priority() &&
          a.is_speculative() == b.is_speculative() &&
          a.referrer() == b.referrer();
}

TestHostResolverObserver::TestHostResolverObserver() {
}

TestHostResolverObserver::~TestHostResolverObserver() {
}

void TestHostResolverObserver::OnStartResolution(
    int id,
    const HostResolver::RequestInfo& info) {
  start_log.push_back(StartOrCancelEntry(id, info));
}

void TestHostResolverObserver::OnFinishResolutionWithStatus(
    int id,
    bool was_resolved,
    const HostResolver::RequestInfo& info) {
  finish_log.push_back(FinishEntry(id, was_resolved, info));
}

void TestHostResolverObserver::OnCancelResolution(
    int id,
    const HostResolver::RequestInfo& info) {
  cancel_log.push_back(StartOrCancelEntry(id, info));
}

}  // namespace
