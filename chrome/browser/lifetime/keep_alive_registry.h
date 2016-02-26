// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_KEEP_ALIVE_REGISTRY_H_
#define CHROME_BROWSER_LIFETIME_KEEP_ALIVE_REGISTRY_H_

#include <map>

#include "base/macros.h"
#include "base/memory/singleton.h"

enum class KeepAliveOrigin;

class KeepAliveRegistry {
 public:
  static KeepAliveRegistry* GetInstance();

  bool WillKeepAlive() const;

 private:
  friend struct base::DefaultSingletonTraits<KeepAliveRegistry>;
  // Friend to be able to use Register/Unregister
  friend class ScopedKeepAlive;

  KeepAliveRegistry();
  ~KeepAliveRegistry();

  // Add/Remove entries. Do not use directly, use ScopedKeepAlive instead.
  void Register(KeepAliveOrigin origin);
  void Unregister(KeepAliveOrigin origin);

  // Tracks the registered KeepAlives, storing the origin and the number of
  // registered KeepAlives for each.
  std::map<KeepAliveOrigin, int> registered_keep_alives_;

  // Total number of registered KeepAlives
  int registered_count_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveRegistry);
};

#endif  // CHROME_BROWSER_LIFETIME_KEEP_ALIVE_REGISTRY_H_
