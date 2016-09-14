// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_intent_helper_instance.h"

namespace arc {

FakeIntentHelperInstance::FakeIntentHelperInstance() {}

FakeIntentHelperInstance::Broadcast::Broadcast(const mojo::String& action,
                                               const mojo::String& package_name,
                                               const mojo::String& cls,
                                               const mojo::String& extras)
    : action(action), package_name(package_name), cls(cls), extras(extras) {}

FakeIntentHelperInstance::Broadcast::Broadcast(const Broadcast& broadcast)
    : action(broadcast.action),
      package_name(broadcast.package_name),
      cls(broadcast.cls),
      extras(broadcast.extras) {}

FakeIntentHelperInstance::Broadcast::~Broadcast() {}

FakeIntentHelperInstance::~FakeIntentHelperInstance() {}

void FakeIntentHelperInstance::AddPreferredPackage(
    const mojo::String& package_name) {}

void FakeIntentHelperInstance::HandleUrl(const mojo::String& url,
                                         const mojo::String& package_name) {}

void FakeIntentHelperInstance::HandleUrlList(
    mojo::Array<mojom::UrlWithMimeTypePtr> urls,
    mojom::ActivityNamePtr activity,
    mojom::ActionType action) {}

void FakeIntentHelperInstance::HandleUrlListDeprecated(
    mojo::Array<mojom::UrlWithMimeTypePtr> urls,
    const mojo::String& package_name,
    mojom::ActionType action) {}

void FakeIntentHelperInstance::Init(mojom::IntentHelperHostPtr host_ptr) {}

void FakeIntentHelperInstance::RequestActivityIcons(
    mojo::Array<mojom::ActivityNamePtr> activities,
    ::arc::mojom::ScaleFactor scale_factor,
    const RequestActivityIconsCallback& callback) {}

void FakeIntentHelperInstance::RequestUrlHandlerList(
    const mojo::String& url,
    const RequestUrlHandlerListCallback& callback) {}

void FakeIntentHelperInstance::RequestUrlListHandlerList(
    mojo::Array<mojom::UrlWithMimeTypePtr> urls,
    const RequestUrlListHandlerListCallback& callback) {}

void FakeIntentHelperInstance::SendBroadcast(const mojo::String& action,
                                             const mojo::String& package_name,
                                             const mojo::String& cls,
                                             const mojo::String& extras) {
  broadcasts_.emplace_back(action, package_name, cls, extras);
}

}  // namespace arc
