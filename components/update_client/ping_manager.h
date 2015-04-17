// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_
#define COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_

#include "base/macros.h"

namespace update_client {

class Configurator;
struct CrxUpdateItem;

// Provides an event sink for completion events from ComponentUpdateService
// and sends fire-and-forget pings when handling these events.
class PingManager {
 public:
  explicit PingManager(const Configurator& config);
  virtual ~PingManager();

  virtual void OnUpdateComplete(const CrxUpdateItem* item);

 private:
  const Configurator& config_;

  DISALLOW_COPY_AND_ASSIGN(PingManager);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_
