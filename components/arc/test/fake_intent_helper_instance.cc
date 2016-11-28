// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_intent_helper_instance.h"

namespace arc {

FakeIntentHelperInstance::FakeIntentHelperInstance() {}

FakeIntentHelperInstance::Broadcast::Broadcast(const std::string& action,
                                               const std::string& package_name,
                                               const std::string& cls,
                                               const std::string& extras)
    : action(action), package_name(package_name), cls(cls), extras(extras) {}

FakeIntentHelperInstance::Broadcast::Broadcast(const Broadcast& broadcast)
    : action(broadcast.action),
      package_name(broadcast.package_name),
      cls(broadcast.cls),
      extras(broadcast.extras) {}

FakeIntentHelperInstance::Broadcast::~Broadcast() {}

FakeIntentHelperInstance::~FakeIntentHelperInstance() {}

void FakeIntentHelperInstance::AddPreferredPackage(
    const std::string& package_name) {}

void FakeIntentHelperInstance::GetFileSizeDeprecated(
    const std::string& url,
    const GetFileSizeDeprecatedCallback& callback) {}

void FakeIntentHelperInstance::HandleIntent(mojom::IntentInfoPtr intent,
                                            mojom::ActivityNamePtr activity) {}

void FakeIntentHelperInstance::HandleUrl(const std::string& url,
                                         const std::string& package_name) {}

void FakeIntentHelperInstance::HandleUrlList(
    std::vector<mojom::UrlWithMimeTypePtr> urls,
    mojom::ActivityNamePtr activity,
    mojom::ActionType action) {}

void FakeIntentHelperInstance::Init(mojom::IntentHelperHostPtr host_ptr) {}

void FakeIntentHelperInstance::OpenFileToReadDeprecated(
    const std::string& url,
    const OpenFileToReadDeprecatedCallback& callback) {}

void FakeIntentHelperInstance::RequestActivityIcons(
    std::vector<mojom::ActivityNamePtr> activities,
    ::arc::mojom::ScaleFactor scale_factor,
    const RequestActivityIconsCallback& callback) {}

void FakeIntentHelperInstance::RequestIntentHandlerList(
    mojom::IntentInfoPtr intent,
    const RequestIntentHandlerListCallback& callback) {}

void FakeIntentHelperInstance::RequestUrlHandlerList(
    const std::string& url,
    const RequestUrlHandlerListCallback& callback) {}

void FakeIntentHelperInstance::RequestUrlListHandlerList(
    std::vector<mojom::UrlWithMimeTypePtr> urls,
    const RequestUrlListHandlerListCallback& callback) {}

void FakeIntentHelperInstance::SendBroadcast(const std::string& action,
                                             const std::string& package_name,
                                             const std::string& cls,
                                             const std::string& extras) {
  broadcasts_.emplace_back(action, package_name, cls, extras);
}

}  // namespace arc
