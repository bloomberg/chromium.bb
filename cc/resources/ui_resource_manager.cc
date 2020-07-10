// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/ui_resource_manager.h"

#include <algorithm>

#include "cc/resources/scoped_ui_resource.h"

namespace cc {

UIResourceManager::UIResourceManager() : next_ui_resource_id_(1) {}

UIResourceManager::~UIResourceManager() = default;

UIResourceId UIResourceManager::CreateUIResource(UIResourceClient* client) {
  DCHECK(client);

  UIResourceId next_id = next_ui_resource_id_++;
  DCHECK(ui_resource_client_map_.find(next_id) ==
         ui_resource_client_map_.end());

  bool resource_lost = false;
  UIResourceRequest request(UIResourceRequest::UI_RESOURCE_CREATE, next_id,
                            client->GetBitmap(next_id, resource_lost));
  ui_resource_request_queue_.push_back(request);

  UIResourceClientData data;
  data.client = client;
  data.size = request.GetBitmap().GetSize();

  ui_resource_client_map_[request.GetId()] = data;
  return request.GetId();
}

void UIResourceManager::DeleteUIResource(UIResourceId uid) {
  const auto iter = ui_resource_client_map_.find(uid);
  if (iter == ui_resource_client_map_.end())
    return;

  UIResourceRequest request(UIResourceRequest::UI_RESOURCE_DELETE, uid);
  ui_resource_request_queue_.push_back(request);
  ui_resource_client_map_.erase(iter);
}

void UIResourceManager::RecreateUIResources() {
  for (const auto& resource : ui_resource_client_map_) {
    UIResourceId uid = resource.first;
    const UIResourceClientData& data = resource.second;
    bool resource_lost = true;
    auto it = std::find_if(ui_resource_request_queue_.begin(),
                           ui_resource_request_queue_.end(),
                           [uid](const UIResourceRequest& request) {
                             return request.GetId() == uid;
                           });
    if (it == ui_resource_request_queue_.end()) {
      UIResourceRequest request(UIResourceRequest::UI_RESOURCE_CREATE, uid,
                                data.client->GetBitmap(uid, resource_lost));
      ui_resource_request_queue_.push_back(request);
    }
  }
}

gfx::Size UIResourceManager::GetUIResourceSize(UIResourceId uid) const {
  const auto iter = ui_resource_client_map_.find(uid);
  if (iter == ui_resource_client_map_.end())
    return gfx::Size();

  const UIResourceClientData& data = iter->second;
  return data.size;
}

std::vector<UIResourceRequest> UIResourceManager::TakeUIResourcesRequests() {
  UIResourceRequestQueue result;
  result.swap(ui_resource_request_queue_);
  return result;
}

UIResourceId UIResourceManager::GetOrCreateUIResource(const SkBitmap& bitmap) {
  DCHECK(bitmap.pixelRef()->isImmutable());
  const auto resource = owned_shared_resources_.find(bitmap.pixelRef());
  if (resource != owned_shared_resources_.end())
    return resource->second->id();

  auto scoped_resource =
      ScopedUIResource::Create(this, UIResourceBitmap(bitmap));
  auto id = scoped_resource->id();
  owned_shared_resources_[bitmap.pixelRef()] = std::move(scoped_resource);
  return id;
}

}  // namespace cc
