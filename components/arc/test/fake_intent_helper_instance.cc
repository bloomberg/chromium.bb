// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_intent_helper_instance.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/thread_task_runner_handle.h"

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

FakeIntentHelperInstance::HandledIntent::HandledIntent(
    mojom::IntentInfoPtr intent,
    mojom::ActivityNamePtr activity)
    : intent(std::move(intent)), activity(std::move(activity)) {}

FakeIntentHelperInstance::HandledIntent::HandledIntent(HandledIntent&& other) =
    default;

FakeIntentHelperInstance::HandledIntent&
FakeIntentHelperInstance::HandledIntent::operator=(HandledIntent&& other) =
    default;

FakeIntentHelperInstance::HandledIntent::~HandledIntent() = default;

void FakeIntentHelperInstance::SetIntentHandlers(
    const std::string& action,
    std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  intent_handlers_[action] = std::move(handlers);
}

FakeIntentHelperInstance::~FakeIntentHelperInstance() {}

void FakeIntentHelperInstance::AddPreferredPackage(
    const std::string& package_name) {}

void FakeIntentHelperInstance::GetFileSizeDeprecated(
    const std::string& url,
    GetFileSizeDeprecatedCallback callback) {}

void FakeIntentHelperInstance::HandleIntent(mojom::IntentInfoPtr intent,
                                            mojom::ActivityNamePtr activity) {
  handled_intents_.emplace_back(std::move(intent), std::move(activity));
}

void FakeIntentHelperInstance::HandleUrl(const std::string& url,
                                         const std::string& package_name) {}

void FakeIntentHelperInstance::HandleUrlList(
    std::vector<mojom::UrlWithMimeTypePtr> urls,
    mojom::ActivityNamePtr activity,
    mojom::ActionType action) {}

void FakeIntentHelperInstance::InitDeprecated(
    mojom::IntentHelperHostPtr host_ptr) {
  Init(std::move(host_ptr), base::BindOnce(&base::DoNothing));
}

void FakeIntentHelperInstance::Init(mojom::IntentHelperHostPtr host_ptr,
                                    InitCallback callback) {
  host_ = std::move(host_ptr);
  std::move(callback).Run();
}

void FakeIntentHelperInstance::OpenFileToReadDeprecated(
    const std::string& url,
    OpenFileToReadDeprecatedCallback callback) {}

void FakeIntentHelperInstance::RequestActivityIcons(
    std::vector<mojom::ActivityNamePtr> activities,
    ::arc::mojom::ScaleFactor scale_factor,
    RequestActivityIconsCallback callback) {}

void FakeIntentHelperInstance::RequestIntentHandlerList(
    mojom::IntentInfoPtr intent,
    RequestIntentHandlerListCallback callback) {
  std::vector<mojom::IntentHandlerInfoPtr> handlers;
  const auto it = intent_handlers_.find(intent->action);
  if (it != intent_handlers_.end()) {
    handlers.reserve(it->second.size());
    for (const auto& handler : it->second) {
      handlers.emplace_back(handler.Clone());
    }
  }
  // Post the reply to run asynchronously to match the real implementation.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), base::Passed(std::move(handlers))));
}

void FakeIntentHelperInstance::RequestUrlHandlerList(
    const std::string& url,
    RequestUrlHandlerListCallback callback) {}

void FakeIntentHelperInstance::RequestUrlListHandlerList(
    std::vector<mojom::UrlWithMimeTypePtr> urls,
    RequestUrlListHandlerListCallback callback) {}

void FakeIntentHelperInstance::SendBroadcast(const std::string& action,
                                             const std::string& package_name,
                                             const std::string& cls,
                                             const std::string& extras) {
  broadcasts_.emplace_back(action, package_name, cls, extras);
}

}  // namespace arc
