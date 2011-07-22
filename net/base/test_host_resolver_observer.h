// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TEST_HOST_RESOLVER_OBSERVER_H_
#define NET_BASE_TEST_HOST_RESOLVER_OBSERVER_H_
#pragma once

#include "net/base/host_resolver.h"

#include <vector>

namespace net {

bool operator==(const HostResolver::RequestInfo& a,
                const HostResolver::RequestInfo& b);

// Observer that just makes note of how it was called. The test code can then
// inspect to make sure it was called with the right parameters.  Used by
// HostResolverImpl and AsyncHostResolver unit tests.
class TestHostResolverObserver : public HostResolver::Observer {
 public:
  TestHostResolverObserver();
  virtual ~TestHostResolverObserver();

  // HostResolver::Observer methods:
  virtual void OnStartResolution(int id, const HostResolver::RequestInfo& info);
  virtual void OnFinishResolutionWithStatus(
      int id,
      bool was_resolved,
      const HostResolver::RequestInfo& info);
  virtual void OnCancelResolution(
      int id,
      const HostResolver::RequestInfo& info);

  // Tuple (id, info).
  struct StartOrCancelEntry {
    StartOrCancelEntry(int id, const HostResolver::RequestInfo& info)
        : id(id), info(info) {}

    bool operator==(const StartOrCancelEntry& other) const {
      return id == other.id && info == other.info;
    }

    int id;
    HostResolver::RequestInfo info;
  };

  // Tuple (id, was_resolved, info).
  struct FinishEntry {
    FinishEntry(int id, bool was_resolved,
                const HostResolver::RequestInfo& info)
        : id(id), was_resolved(was_resolved), info(info) {}

    bool operator==(const FinishEntry& other) const {
      return id == other.id &&
             was_resolved == other.was_resolved &&
             info == other.info;
    }

    int id;
    bool was_resolved;
    HostResolver::RequestInfo info;
  };

  std::vector<StartOrCancelEntry> start_log;
  std::vector<FinishEntry> finish_log;
  std::vector<StartOrCancelEntry> cancel_log;
};

}  // namespace net

#endif  // NET_BASE_TEST_HOST_RESOLVER_OBSERVER_H_
