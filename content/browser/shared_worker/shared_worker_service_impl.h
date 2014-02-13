// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/worker_service.h"

namespace content {

class WorkerServiceObserver;

// If "enable-embedded-shared-worker" is set this class will be used in stead of
// WorkerServiceImpl.
// TODO(horo): implement this class.
class CONTENT_EXPORT SharedWorkerServiceImpl
    : public NON_EXPORTED_BASE(WorkerService) {
 public:
  // Returns the SharedWorkerServiceImpl singleton.
  static SharedWorkerServiceImpl* GetInstance();

  // WorkerService implementation:
  virtual bool TerminateWorker(int process_id, int route_id) OVERRIDE;
  virtual std::vector<WorkerInfo> GetWorkers() OVERRIDE;
  virtual void AddObserver(WorkerServiceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(WorkerServiceObserver* observer) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<SharedWorkerServiceImpl>;

  SharedWorkerServiceImpl();
  virtual ~SharedWorkerServiceImpl();

  ObserverList<WorkerServiceObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SERVICE_IMPL_H_
