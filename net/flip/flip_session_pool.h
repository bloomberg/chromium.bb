// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FLIP_FLIP_SESSION_POOL_H_
#define NET_FLIP_FLIP_SESSION_POOL_H_

#include <map>
#include <list>
#include <string>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/host_resolver.h"

namespace net {

class FlipSession;
class HttpNetworkSession;

// This is a very simple pool for open FlipSessions.
// TODO(mbelshe): Make this production ready.
class FlipSessionPool {
 public:
  FlipSessionPool() {}
  virtual ~FlipSessionPool() {}

  // Factory for finding open sessions.
  FlipSession* Get(const HostResolver::RequestInfo& info,
                   HttpNetworkSession* session);

  // Close all Flip Sessions; used for debugging.
  static void CloseAllSessions();

 protected:
  friend class FlipSession;

  // Return a FlipSession to the pool.
  void Remove(FlipSession* session);

 private:
  typedef std::list<FlipSession*> FlipSessionList;
  typedef std::map<std::string, FlipSessionList*> FlipSessionsMap;

  // Helper functions for manipulating the lists.
  FlipSessionList* AddSessionList(std::string domain);
  static FlipSessionList* GetSessionList(std::string domain);
  static void RemoveSessionList(std::string domain);

  // This is our weak session pool - one session per domain.
  static scoped_ptr<FlipSessionsMap> sessions_;
};

}  // namespace net

#endif  // NET_FLIP_FLIP_SESSION_POOL_H_

