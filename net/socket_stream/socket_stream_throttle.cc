// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/socket_stream/socket_stream_throttle.h"

#include "base/hash_tables.h"
#include "base/singleton.h"
#include "net/base/completion_callback.h"
#include "net/socket_stream/socket_stream.h"

namespace net {

// Default SocketStreamThrottle.  No throttling. Used for unknown URL scheme.
class DefaultSocketStreamThrottle : public SocketStreamThrottle {
 private:
  DefaultSocketStreamThrottle() {}
  virtual ~DefaultSocketStreamThrottle() {}
  friend struct DefaultSingletonTraits<DefaultSocketStreamThrottle>;

  DISALLOW_COPY_AND_ASSIGN(DefaultSocketStreamThrottle);
};

class SocketStreamThrottleRegistry {
 public:
  SocketStreamThrottle* GetSocketStreamThrottleForScheme(
      const std::string& scheme);

  void RegisterSocketStreamThrottle(
      const std::string& scheme, SocketStreamThrottle* throttle);

 private:
  typedef base::hash_map<std::string, SocketStreamThrottle*> ThrottleMap;

  SocketStreamThrottleRegistry() {}
  ~SocketStreamThrottleRegistry() {}
  friend struct DefaultSingletonTraits<SocketStreamThrottleRegistry>;

  ThrottleMap throttles_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamThrottleRegistry);
};

SocketStreamThrottle*
SocketStreamThrottleRegistry::GetSocketStreamThrottleForScheme(
    const std::string& scheme) {
  ThrottleMap::const_iterator found = throttles_.find(scheme);
  if (found == throttles_.end()) {
    SocketStreamThrottle* throttle =
        Singleton<DefaultSocketStreamThrottle>::get();
    throttles_[scheme] = throttle;
    return throttle;
  }
  return found->second;
}

void SocketStreamThrottleRegistry::RegisterSocketStreamThrottle(
    const std::string& scheme, SocketStreamThrottle* throttle) {
  throttles_[scheme] = throttle;
}

/* static */
SocketStreamThrottle* SocketStreamThrottle::GetSocketStreamThrottleForScheme(
    const std::string& scheme) {
  SocketStreamThrottleRegistry* registry =
      Singleton<SocketStreamThrottleRegistry>::get();
  return registry->GetSocketStreamThrottleForScheme(scheme);
}

/* static */
void SocketStreamThrottle::RegisterSocketStreamThrottle(
    const std::string& scheme, SocketStreamThrottle* throttle) {
  SocketStreamThrottleRegistry* registry =
      Singleton<SocketStreamThrottleRegistry>::get();
  registry->RegisterSocketStreamThrottle(scheme, throttle);
}

}  // namespace net
