// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_FACTORY_H_
#define CONTENT_CHILD_SERVICE_FACTORY_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "content/public/common/mojo_application_info.h"
#include "services/shell/public/interfaces/service.mojom.h"
#include "services/shell/public/interfaces/service_factory.mojom.h"

namespace content {

class EmbeddedApplicationRunner;

// Base class for child-process specific implementations of
// shell::mojom::ServiceFactory.
class ServiceFactory : public shell::mojom::ServiceFactory {
 public:
  using ServiceMap = std::map<std::string, MojoApplicationInfo>;

  ServiceFactory();
  ~ServiceFactory() override;

  virtual void RegisterServices(ServiceMap* services) = 0;
  virtual void OnServiceQuit() {}

  // shell::mojom::ServiceFactory:
  void CreateService(shell::mojom::ServiceRequest request,
                     const std::string& name) override;

 private:
  // Called if CreateService fails to find a registered service.
  virtual void OnLoadFailed() {}

  bool has_registered_services_ = false;
  std::unordered_map<std::string, std::unique_ptr<EmbeddedApplicationRunner>>
      services_;

  DISALLOW_COPY_AND_ASSIGN(ServiceFactory);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_FACTORY_H_
