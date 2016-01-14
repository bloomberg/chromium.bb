// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_
#define COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace update_client {

class Configurator;
struct CrxUpdateItem;

// Sends fire-and-forget pings.
class PingManager {
 public:
  explicit PingManager(const scoped_refptr<Configurator>& config);
  virtual ~PingManager();

  virtual void SendPing(const CrxUpdateItem* item);

 private:
  const scoped_refptr<Configurator> config_;

  DISALLOW_COPY_AND_ASSIGN(PingManager);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_
