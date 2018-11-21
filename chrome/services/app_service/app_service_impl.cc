// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/app_service_impl.h"

#include <utility>

#include "mojo/public/cpp/bindings/interface_request.h"

namespace {

void Connect(apps::mojom::Publisher* publisher,
             apps::mojom::Subscriber* subscriber) {
  apps::mojom::SubscriberPtr clone;
  subscriber->Clone(mojo::MakeRequest(&clone));
  // TODO: replace nullptr with a ConnectOptions.
  publisher->Connect(std::move(clone), nullptr);
}

}  // namespace

namespace apps {

AppServiceImpl::AppServiceImpl() = default;

AppServiceImpl::~AppServiceImpl() = default;

void AppServiceImpl::BindRequest(apps::mojom::AppServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AppServiceImpl::RegisterPublisher(apps::mojom::PublisherPtr publisher,
                                       apps::mojom::AppType app_type) {
  // Connect the new publisher with every registered subscriber.
  subscribers_.ForAllPtrs([&publisher](auto* subscriber) {
    ::Connect(publisher.get(), subscriber);
  });

  // Check that no previous publisher has registered for the same app_type.
  CHECK(publishers_by_type_.find(app_type) == publishers_by_type_.end());

  // Add the new publisher to the set.
  //
  // We also track publishers by their app_type, so that we can forward other
  // App Service method calls on to one particular publisher, instead of
  // broadcasting to all publishers.
  publishers_by_type_[app_type] = publishers_.AddPtr(std::move(publisher));
}

void AppServiceImpl::RegisterSubscriber(apps::mojom::SubscriberPtr subscriber,
                                        apps::mojom::ConnectOptionsPtr opts) {
  // Connect the new subscriber with every registered publisher.
  publishers_.ForAllPtrs([&subscriber](auto* publisher) {
    ::Connect(publisher, subscriber.get());
  });

  // TODO: store the opts somewhere.

  // Add the new subscriber to the set.
  subscribers_.AddPtr(std::move(subscriber));
}

}  // namespace apps
