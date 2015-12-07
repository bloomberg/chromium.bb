// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_
#define COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace base {

class SequencedTaskRunner;
class SingleThreadTaskRunner;

}  // namespace base

namespace arc {

class ArcBridgeService;

// Manages creation and destruction of services that communicate with the ARC
// instance via the ArcBridgeService.
class ArcServiceManager {
 public:
  ArcServiceManager(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& file_task_runner);
  virtual ~ArcServiceManager();

  // |arc_bridge_service| can only be accessed on the thread that this
  // class was created on.
  ArcBridgeService* arc_bridge_service();

  // Gets the global instance of the ARC Service Manager. This can only be
  // called on the thread that this class was created on.
  static ArcServiceManager* Get();

 private:
  scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  scoped_ptr<ArcBridgeService> arc_bridge_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcServiceManager);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_
