// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/link_handler_model_impl.h"

#include <vector>

#include "base/bind.h"
#include "components/arc/arc_bridge_service.h"
#include "url/gurl.h"

namespace arc {

namespace {

const int kMinInstanceVersion = 2;  // see intent_helper.mojom

}  // namespace

LinkHandlerModelImpl::LinkHandlerModelImpl() : weak_ptr_factory_(this) {}

LinkHandlerModelImpl::~LinkHandlerModelImpl() {}

bool LinkHandlerModelImpl::Init(const GURL& url) {
  mojom::IntentHelperInstance* intent_helper_instance = GetIntentHelper();
  if (!intent_helper_instance)
    return false;

  // Check if ARC apps can handle the |url|. Since the information is held in
  // a different (ARC) process, issue a mojo IPC request. Usually, the
  // callback function, OnUrlHandlerList, is called within a few milliseconds
  // even on a slowest Chromebook we support.
  intent_helper_instance->RequestUrlHandlerList(
      url.spec(), base::Bind(&LinkHandlerModelImpl::OnUrlHandlerList,
                             weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void LinkHandlerModelImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LinkHandlerModelImpl::OpenLinkWithHandler(const GURL& url,
                                               uint32_t handler_id) {
  mojom::IntentHelperInstance* intent_helper_instance = GetIntentHelper();
  if (!intent_helper_instance)
    return;
  if (handler_id >= handlers_.size())
    return;
  intent_helper_instance->HandleUrl(url.spec(),
                                    handlers_[handler_id]->package_name);
}

mojom::IntentHelperInstance* LinkHandlerModelImpl::GetIntentHelper() {
  ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    DLOG(WARNING) << "ARC bridge is not ready.";
    return nullptr;
  }
  mojom::IntentHelperInstance* intent_helper_instance =
      bridge_service->intent_helper_instance();
  if (!intent_helper_instance) {
    DLOG(WARNING) << "ARC intent helper instance is not ready.";
    return nullptr;
  }
  if (bridge_service->intent_helper_version() < kMinInstanceVersion) {
    DLOG(WARNING) << "ARC intent helper instance is too old.";
    return nullptr;
  }
  return intent_helper_instance;
}

void LinkHandlerModelImpl::OnUrlHandlerList(
    mojo::Array<mojom::UrlHandlerInfoPtr> handlers) {
  handlers_ = std::move(handlers);
  NotifyObserver();
}

void LinkHandlerModelImpl::NotifyObserver() {
  std::vector<ash::LinkHandlerInfo> handlers;
  for (size_t i = 0; i < handlers_.size(); ++i) {
    // Use the handler's index as an ID.
    ash::LinkHandlerInfo handler = {handlers_[i]->name.get(), gfx::Image(), i};
    handlers.push_back(handler);
  }
  FOR_EACH_OBSERVER(Observer, observer_list_, ModelChanged(handlers));
}

}  // namespace arc
