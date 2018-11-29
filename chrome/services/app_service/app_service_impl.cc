// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/app_service_impl.h"

#include <utility>

#include "base/bind.h"
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
  CHECK(publishers_.find(app_type) == publishers_.end());

  // Add the new publisher to the set.
  publisher.set_connection_error_handler(
      base::BindOnce(&AppServiceImpl::OnPublisherDisconnected,
                     base::Unretained(this), app_type));
  auto result = publishers_.emplace(app_type, std::move(publisher));
  CHECK(result.second);
}

void AppServiceImpl::RegisterSubscriber(apps::mojom::SubscriberPtr subscriber,
                                        apps::mojom::ConnectOptionsPtr opts) {
  // Connect the new subscriber with every registered publisher.
  for (const auto& iter : publishers_) {
    ::Connect(iter.second.get(), subscriber.get());
  }

  // TODO: store the opts somewhere.

  // Add the new subscriber to the set.
  subscribers_.AddPtr(std::move(subscriber));
}

void AppServiceImpl::LoadIcon(apps::mojom::AppType app_type,
                              const std::string& app_id,
                              apps::mojom::IconKeyPtr icon_key,
                              apps::mojom::IconCompression icon_compression,
                              int32_t size_hint_in_dip,
                              LoadIconCallback callback) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }
  iter->second->LoadIcon(app_id, std::move(icon_key), icon_compression,
                         size_hint_in_dip, std::move(callback));
}

void AppServiceImpl::Launch(apps::mojom::AppType app_type,
                            const std::string& app_id,
                            int32_t event_flags) {
  auto iter = publishers_.find(app_type);
  if (iter == publishers_.end()) {
    return;
  }
  iter->second->Launch(app_id, event_flags);
}

void AppServiceImpl::OnPublisherDisconnected(apps::mojom::AppType app_type) {
  publishers_.erase(app_type);
}

}  // namespace apps
