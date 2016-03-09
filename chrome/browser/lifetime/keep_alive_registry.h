// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_KEEP_ALIVE_REGISTRY_H_
#define CHROME_BROWSER_LIFETIME_KEEP_ALIVE_REGISTRY_H_

#include <map>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"

enum class KeepAliveOrigin;
enum class KeepAliveRestartOption;
class KeepAliveStateObserver;

class KeepAliveRegistry {
 public:
  static KeepAliveRegistry* GetInstance();

  // Methods to query the state of the registry.
  // TODO(dgn): This currently does not give a complete picture. It has no
  // information about the many places that rely on IncrementKeepAliveCount and
  // AddRefModule to keep the browser alive. Tracked by https://crbug.com/587926
  bool IsKeepingAlive() const;
  bool IsRestartAllowed() const;

  void AddObserver(KeepAliveStateObserver* observer);
  void RemoveObserver(KeepAliveStateObserver* observer);

 private:
  friend struct base::DefaultSingletonTraits<KeepAliveRegistry>;
  // Friend to be able to use Register/Unregister
  friend class ScopedKeepAlive;
  friend std::ostream& operator<<(std::ostream& out,
                                  const KeepAliveRegistry& registry);

  KeepAliveRegistry();
  ~KeepAliveRegistry();

  // Add/Remove entries. Do not use directly, use ScopedKeepAlive instead.
  void Register(KeepAliveOrigin origin, KeepAliveRestartOption restart);
  void Unregister(KeepAliveOrigin origin, KeepAliveRestartOption restart);

  // Methods called when a specific aspect of the state of the registry changes.
  void OnKeepingAliveChanged(bool new_keeping_alive);
  void OnRestartAllowedChanged(bool new_restart_allowed);

  // Tracks the registered KeepAlives, storing the origin and the number of
  // registered KeepAlives for each.
  std::map<KeepAliveOrigin, int> registered_keep_alives_;

  // Total number of registered KeepAlives
  int registered_count_;

  // Number of registered keep alives that have KeepAliveRestartOption::ENABLED.
  int restart_allowed_count_;

  base::ObserverList<KeepAliveStateObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveRegistry);
};

#ifndef NDEBUG
std::ostream& operator<<(std::ostream& out, const KeepAliveRegistry& registry);
#endif  // ndef NDEBUG

#endif  // CHROME_BROWSER_LIFETIME_KEEP_ALIVE_REGISTRY_H_
