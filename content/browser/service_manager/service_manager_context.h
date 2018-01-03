// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_MANAGER_SERVICE_MANAGER_CONTEXT_H_
#define CONTENT_BROWSER_SERVICE_MANAGER_SERVICE_MANAGER_CONTEXT_H_

#include <map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace service_manager {
class Connector;
}

namespace content {

class ServiceManagerConnection;
class UtilityProcessHost;

// ServiceManagerContext manages the browser's connection to the ServiceManager,
// hosting a new in-process ServiceManagerContext if the browser was not
// launched from an external one.
class CONTENT_EXPORT ServiceManagerContext {
 public:
  ServiceManagerContext();
  ~ServiceManagerContext();

  // Returns a service_manager::Connector that can be used on the IO thread.
  static service_manager::Connector* GetConnectorForIOThread();

  static std::map<std::string, base::WeakPtr<UtilityProcessHost>>*
  GetProcessGroupsForTesting();

 private:
  class InProcessServiceManagerContext;

  scoped_refptr<InProcessServiceManagerContext> in_process_context_;
  std::unique_ptr<ServiceManagerConnection> packaged_services_connection_;

  DISALLOW_COPY_AND_ASSIGN(ServiceManagerContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_MANAGER_SERVICE_MANAGER_CONTEXT_H_
