// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_PING_MANAGER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_PING_MANAGER_H_

#include "base/basictypes.h"

namespace component_updater {

class Configurator;
struct CrxUpdateItem;

// Provides an event sink for completion events from ComponentUpdateService
// and sends fire-and-forget pings when handling these events.
class PingManager {
 public:
  explicit PingManager(const Configurator& config);
  ~PingManager();

  void OnUpdateComplete(const CrxUpdateItem* item);

 private:
  const Configurator& config_;

  DISALLOW_COPY_AND_ASSIGN(PingManager);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UPDATER_PING_MANAGER_H_
