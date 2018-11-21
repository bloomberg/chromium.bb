// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_
#define CHROME_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_

#include <map>

#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace apps {

// The implementation of the apps::mojom::AppService Mojo interface. For the
// service (in the service_manager::Service sense) aspect of the App Service,
// see the AppService class.
//
// See chrome/services/app_service/README.md.
class AppServiceImpl : public apps::mojom::AppService {
 public:
  AppServiceImpl();
  ~AppServiceImpl() override;

  void BindRequest(apps::mojom::AppServiceRequest request);

  // apps::mojom::AppService overrides.
  void RegisterPublisher(apps::mojom::PublisherPtr publisher,
                         apps::mojom::AppType app_type) override;
  void RegisterSubscriber(apps::mojom::SubscriberPtr subscriber,
                          apps::mojom::ConnectOptionsPtr opts) override;

 private:
  mojo::BindingSet<apps::mojom::AppService> bindings_;

  std::map<apps::mojom::AppType, mojo::InterfacePtrSetElementId>
      publishers_by_type_;
  mojo::InterfacePtrSet<apps::mojom::Publisher> publishers_;
  mojo::InterfacePtrSet<apps::mojom::Subscriber> subscribers_;

  DISALLOW_COPY_AND_ASSIGN(AppServiceImpl);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_APP_SERVICE_IMPL_H_
