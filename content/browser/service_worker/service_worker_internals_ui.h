// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/web_ui_controller.h"

namespace base {
class ListValue;
class DictionaryValue;
}

namespace content {

class StoragePartition;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;

class ServiceWorkerInternalsUI
    : public WebUIController,
      public base::SupportsWeakPtr<ServiceWorkerInternalsUI> {
 public:
  explicit ServiceWorkerInternalsUI(WebUI* web_ui);

 private:
  class OperationProxy;

  virtual ~ServiceWorkerInternalsUI();
  void AddContextFromStoragePartition(StoragePartition* partition);

  // Called from Javascript.
  void GetAllRegistrations(const base::ListValue* args);
  void StartWorker(const base::ListValue* args);
  void StopWorker(const base::ListValue* args);
  void Unregister(const base::ListValue* args);

  bool GetRegistrationInfo(
      const base::ListValue* args,
      base::FilePath* partition_path,
      GURL* scope,
      scoped_refptr<ServiceWorkerContextWrapper>* context) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_INTERNALS_UI_H_
