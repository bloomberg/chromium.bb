// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "third_party/WebKit/common/feature_policy/feature_policy_feature.h"

using blink::mojom::PermissionDescriptorPtr;
using blink::mojom::PermissionName;
using blink::mojom::PermissionObserverPtr;
using blink::mojom::PermissionStatus;

namespace content {

namespace {

PermissionType PermissionDescriptorToPermissionType(
    const PermissionDescriptorPtr& descriptor) {
  switch (descriptor->name) {
    case PermissionName::GEOLOCATION:
      return PermissionType::GEOLOCATION;
    case PermissionName::NOTIFICATIONS:
      return PermissionType::NOTIFICATIONS;
    case PermissionName::PUSH_NOTIFICATIONS:
      return PermissionType::PUSH_MESSAGING;
    case PermissionName::MIDI: {
      if (descriptor->extension && descriptor->extension->is_midi() &&
          descriptor->extension->get_midi()->sysex) {
        return PermissionType::MIDI_SYSEX;
      }
      return PermissionType::MIDI;
    }
    case PermissionName::PROTECTED_MEDIA_IDENTIFIER:
      return PermissionType::PROTECTED_MEDIA_IDENTIFIER;
    case PermissionName::DURABLE_STORAGE:
      return PermissionType::DURABLE_STORAGE;
    case PermissionName::AUDIO_CAPTURE:
      return PermissionType::AUDIO_CAPTURE;
    case PermissionName::VIDEO_CAPTURE:
      return PermissionType::VIDEO_CAPTURE;
    case PermissionName::BACKGROUND_SYNC:
      return PermissionType::BACKGROUND_SYNC;
    case PermissionName::SENSORS:
      return PermissionType::SENSORS;
    case PermissionName::ACCESSIBILITY_EVENTS:
      return PermissionType::ACCESSIBILITY_EVENTS;
    case PermissionName::CLIPBOARD_READ:
    case PermissionName::CLIPBOARD_WRITE:
      NOTIMPLEMENTED();
      break;
  }

  NOTREACHED();
  return PermissionType::NUM;
}

blink::WebFeaturePolicyFeature PermissionTypeToFeaturePolicyFeature(
    PermissionType type) {
  switch (type) {
    case PermissionType::MIDI:
    case PermissionType::MIDI_SYSEX:
      return blink::WebFeaturePolicyFeature::kMidiFeature;
    case PermissionType::GEOLOCATION:
      return blink::WebFeaturePolicyFeature::kGeolocation;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return blink::WebFeaturePolicyFeature::kEncryptedMedia;
    case PermissionType::AUDIO_CAPTURE:
      return blink::WebFeaturePolicyFeature::kMicrophone;
    case PermissionType::VIDEO_CAPTURE:
      return blink::WebFeaturePolicyFeature::kCamera;
    case PermissionType::PUSH_MESSAGING:
    case PermissionType::NOTIFICATIONS:
    case PermissionType::DURABLE_STORAGE:
    case PermissionType::BACKGROUND_SYNC:
    case PermissionType::FLASH:
    case PermissionType::SENSORS:
    case PermissionType::ACCESSIBILITY_EVENTS:
    case PermissionType::NUM:
      // These aren't exposed by feature policy.
      return blink::WebFeaturePolicyFeature::kNotFound;
  }

  NOTREACHED();
  return blink::WebFeaturePolicyFeature::kNotFound;
}

bool AllowedByFeaturePolicy(RenderFrameHost* rfh, PermissionType type) {
  if (!base::FeatureList::IsEnabled(
          features::kUseFeaturePolicyForPermissions)) {
    // Default to ignoring the feature policy.
    return true;
  }

  blink::WebFeaturePolicyFeature feature_policy_feature =
      PermissionTypeToFeaturePolicyFeature(type);
  if (feature_policy_feature == blink::WebFeaturePolicyFeature::kNotFound)
    return true;

  return rfh->IsFeatureEnabled(feature_policy_feature);
}

// This function allows the usage of the the multiple request map with single
// requests.
void PermissionRequestResponseCallbackWrapper(
    base::OnceCallback<void(PermissionStatus)> callback,
    const std::vector<PermissionStatus>& vector) {
  DCHECK_EQ(vector.size(), 1ul);
  std::move(callback).Run(vector[0]);
}

}  // anonymous namespace

class PermissionServiceImpl::PendingRequest {
 public:
  PendingRequest(std::vector<PermissionType> types,
                 RequestPermissionsCallback callback)
      : types_(types),
        callback_(std::move(callback)),
        has_result_been_set_(types.size(), false),
        results_(types.size(), PermissionStatus::DENIED) {}

  ~PendingRequest() {
    if (callback_.is_null())
      return;

    std::vector<PermissionStatus> result(types_.size(),
                                         PermissionStatus::DENIED);
    std::move(callback_).Run(result);
  }

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  size_t RequestSize() const { return types_.size(); }

  void SetResult(int index, PermissionStatus result) {
    DCHECK_EQ(false, has_result_been_set_[index]);
    has_result_been_set_[index] = true;
    results_[index] = result;
  }

  bool HasResultBeenSet(size_t index) const {
    return has_result_been_set_[index];
  }

  void RunCallback() {
    // Check that all results have been set.
    DCHECK(std::find(has_result_been_set_.begin(), has_result_been_set_.end(),
                     false) == has_result_been_set_.end());
    std::move(callback_).Run(results_);
  }

 private:
  // Request ID received from the PermissionManager.
  int id_;
  std::vector<PermissionType> types_;
  RequestPermissionsCallback callback_;

  std::vector<bool> has_result_been_set_;
  std::vector<PermissionStatus> results_;
};

PermissionServiceImpl::PermissionServiceImpl(PermissionServiceContext* context)
    : context_(context), weak_factory_(this) {}

PermissionServiceImpl::~PermissionServiceImpl() {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context)
    return;

  PermissionManager* permission_manager =
      browser_context->GetPermissionManager();
  if (!permission_manager)
    return;

  // Cancel pending requests.
  for (RequestsMap::Iterator<PendingRequest> it(&pending_requests_);
       !it.IsAtEnd(); it.Advance()) {
    permission_manager->CancelPermissionRequest(it.GetCurrentValue()->id());
  }
  pending_requests_.Clear();
}

void PermissionServiceImpl::RequestPermission(
    PermissionDescriptorPtr permission,
    const url::Origin& origin,
    bool user_gesture,
    PermissionStatusCallback callback) {
  std::vector<PermissionDescriptorPtr> permissions;
  permissions.push_back(std::move(permission));
  RequestPermissions(std::move(permissions), origin, user_gesture,
                     base::BindOnce(&PermissionRequestResponseCallbackWrapper,
                                    base::Passed(&callback)));
}

void PermissionServiceImpl::RequestPermissions(
    std::vector<PermissionDescriptorPtr> permissions,
    const url::Origin& origin,
    bool user_gesture,
    RequestPermissionsCallback callback) {
  // This condition is valid if the call is coming from a ChildThread instead of
  // a RenderFrame. Some consumers of the service run in Workers and some in
  // Frames. In the context of a Worker, it is not possible to show a
  // permission prompt because there is no tab. In the context of a Frame, we
  // can. Even if the call comes from a context where it is not possible to show
  // any UI, we want to still return something relevant so the current
  // permission status is returned for each permission.
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context)
    return;

  if (!context_->render_frame_host() ||
      !browser_context->GetPermissionManager()) {
    std::vector<PermissionStatus> result(permissions.size());
    for (size_t i = 0; i < permissions.size(); ++i)
      result[i] = GetPermissionStatus(permissions[i], origin);
    std::move(callback).Run(result);
    return;
  }

  std::vector<PermissionType> types(permissions.size());
  for (size_t i = 0; i < types.size(); ++i)
    types[i] = PermissionDescriptorToPermissionType(permissions[i]);

  std::unique_ptr<PendingRequest> pending_request =
      std::make_unique<PendingRequest>(types, std::move(callback));
  std::vector<PermissionType> request_types;
  for (size_t i = 0; i < types.size(); ++i) {
    // Check feature policy.
    if (!AllowedByFeaturePolicy(context_->render_frame_host(), types[i]))
      pending_request->SetResult(i, PermissionStatus::DENIED);
    else
      request_types.push_back(types[i]);
  }

  int pending_request_id = pending_requests_.Add(std::move(pending_request));
  int id = browser_context->GetPermissionManager()->RequestPermissions(
      request_types, context_->render_frame_host(), origin.GetURL(),
      user_gesture,
      base::Bind(&PermissionServiceImpl::OnRequestPermissionsResponse,
                 weak_factory_.GetWeakPtr(), pending_request_id));

  // Check if the request still exists. It may have been removed by the
  // the response callback.
  PendingRequest* in_progress_request =
      pending_requests_.Lookup(pending_request_id);
  if (!in_progress_request)
    return;
  in_progress_request->set_id(id);
}

void PermissionServiceImpl::OnRequestPermissionsResponse(
    int pending_request_id,
    const std::vector<PermissionStatus>& partial_result) {
  PendingRequest* request = pending_requests_.Lookup(pending_request_id);
  auto partial_result_it = partial_result.begin();
  // Fill in the unset results in the request. Some results in the request are
  // set synchronously because they are blocked by feature policy. Others are
  // determined by a call to RequestPermission. All unset results will be
  // contained in |partial_result| in the same order that they were requested.
  // We fill in the unset results in the request with |partial_result|.
  for (size_t i = 0; i < request->RequestSize(); ++i) {
    if (!request->HasResultBeenSet(i)) {
      request->SetResult(i, *partial_result_it);
      ++partial_result_it;
    }
  }
  DCHECK(partial_result.end() == partial_result_it);

  request->RunCallback();
  pending_requests_.Remove(pending_request_id);
}

void PermissionServiceImpl::HasPermission(PermissionDescriptorPtr permission,
                                          const url::Origin& origin,
                                          PermissionStatusCallback callback) {
  std::move(callback).Run(GetPermissionStatus(permission, origin));
}

void PermissionServiceImpl::RevokePermission(
    PermissionDescriptorPtr permission,
    const url::Origin& origin,
    PermissionStatusCallback callback) {
  PermissionType permission_type =
      PermissionDescriptorToPermissionType(permission);
  PermissionStatus status =
      GetPermissionStatusFromType(permission_type, origin);

  // Resetting the permission should only be possible if the permission is
  // already granted.
  if (status != PermissionStatus::GRANTED) {
    std::move(callback).Run(status);
    return;
  }

  ResetPermissionStatus(permission_type, origin);

  std::move(callback).Run(GetPermissionStatusFromType(permission_type, origin));
}

void PermissionServiceImpl::AddPermissionObserver(
    PermissionDescriptorPtr permission,
    const url::Origin& origin,
    PermissionStatus last_known_status,
    PermissionObserverPtr observer) {
  PermissionStatus current_status = GetPermissionStatus(permission, origin);
  if (current_status != last_known_status) {
    observer->OnPermissionStatusChange(current_status);
    last_known_status = current_status;
  }

  context_->CreateSubscription(PermissionDescriptorToPermissionType(permission),
                               origin, std::move(observer));
}

PermissionStatus PermissionServiceImpl::GetPermissionStatus(
    const PermissionDescriptorPtr& permission,
    const url::Origin& origin) {
  return GetPermissionStatusFromType(
      PermissionDescriptorToPermissionType(permission), origin);
}

PermissionStatus PermissionServiceImpl::GetPermissionStatusFromType(
    PermissionType type,
    const url::Origin& origin) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context)
    return PermissionStatus::DENIED;

  if (!browser_context->GetPermissionManager())
    return PermissionStatus::DENIED;

  // If there is no frame (i.e. this is a worker) ignore the feature policy.
  if (context_->render_frame_host() &&
      !AllowedByFeaturePolicy(context_->render_frame_host(), type)) {
    return PermissionStatus::DENIED;
  }

  GURL requesting_origin(origin.Serialize());
  // If the embedding_origin is empty we'll use |origin| instead.
  GURL embedding_origin = context_->GetEmbeddingOrigin();
  return browser_context->GetPermissionManager()->GetPermissionStatus(
      type, requesting_origin,
      embedding_origin.is_empty() ? requesting_origin : embedding_origin);
}

void PermissionServiceImpl::ResetPermissionStatus(PermissionType type,
                                                  const url::Origin& origin) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context)
    return;

  if (!browser_context->GetPermissionManager())
    return;

  GURL requesting_origin(origin.Serialize());
  // If the embedding_origin is empty we'll use |origin| instead.
  GURL embedding_origin = context_->GetEmbeddingOrigin();
  browser_context->GetPermissionManager()->ResetPermission(
      type, requesting_origin,
      embedding_origin.is_empty() ? requesting_origin : embedding_origin);
}

}  // namespace content
