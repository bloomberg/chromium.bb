// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_service_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/background_fetch/background_fetch_job_controller.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/browser/bad_message.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/origin.h"

namespace content {

namespace {

// Maximum length of a developer-provided tag for a Background Fetch.
constexpr size_t kMaxTagLength = 1024 * 1024;

// Maximum length of a developer-provided title for a Background Fetch.
constexpr size_t kMaxTitleLength = 1024 * 1024;

}  // namespace

// static
void BackgroundFetchServiceImpl::Create(
    int render_process_id,
    scoped_refptr<BackgroundFetchContext> background_fetch_context,
    const service_manager::BindSourceInfo& source_info,
    blink::mojom::BackgroundFetchServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeStrongBinding(
      base::MakeUnique<BackgroundFetchServiceImpl>(
          render_process_id, std::move(background_fetch_context)),
      std::move(request));
}

BackgroundFetchServiceImpl::BackgroundFetchServiceImpl(
    int render_process_id,
    scoped_refptr<BackgroundFetchContext> background_fetch_context)
    : render_process_id_(render_process_id),
      background_fetch_context_(std::move(background_fetch_context)) {
  DCHECK(background_fetch_context_);
}

BackgroundFetchServiceImpl::~BackgroundFetchServiceImpl() = default;

void BackgroundFetchServiceImpl::Fetch(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& tag,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    const FetchCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateTag(tag)) {
    callback.Run(blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
                 base::nullopt /* registration */);
    return;
  }

  if (!ValidateRequests(requests)) {
    callback.Run(blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
                 base::nullopt /* registration */);
    return;
  }

  BackgroundFetchRegistrationId registration_id(service_worker_registration_id,
                                                origin, tag);

  background_fetch_context_->StartFetch(registration_id, requests, options,
                                        callback);
}

void BackgroundFetchServiceImpl::UpdateUI(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& tag,
    const std::string& title,
    const UpdateUICallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateTag(tag) || !ValidateTitle(title)) {
    callback.Run(blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  BackgroundFetchJobController* controller =
      background_fetch_context_->GetActiveFetch(BackgroundFetchRegistrationId(
          service_worker_registration_id, origin, tag));

  if (controller)
    controller->UpdateUI(title);

  callback.Run(controller ? blink::mojom::BackgroundFetchError::NONE
                          : blink::mojom::BackgroundFetchError::INVALID_TAG);
}

void BackgroundFetchServiceImpl::Abort(int64_t service_worker_registration_id,
                                       const url::Origin& origin,
                                       const std::string& tag,
                                       const AbortCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateTag(tag)) {
    callback.Run(blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  BackgroundFetchJobController* controller =
      background_fetch_context_->GetActiveFetch(BackgroundFetchRegistrationId(
          service_worker_registration_id, origin, tag));

  if (controller)
    controller->Abort();

  callback.Run(controller ? blink::mojom::BackgroundFetchError::NONE
                          : blink::mojom::BackgroundFetchError::INVALID_TAG);
}

void BackgroundFetchServiceImpl::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& tag,
    const GetRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateTag(tag)) {
    callback.Run(blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
                 base::nullopt /* registration */);
    return;
  }

  BackgroundFetchJobController* controller =
      background_fetch_context_->GetActiveFetch(BackgroundFetchRegistrationId(
          service_worker_registration_id, origin, tag));

  if (!controller) {
    callback.Run(blink::mojom::BackgroundFetchError::INVALID_TAG,
                 base::nullopt /* registration */);
    return;
  }

  // Compile the BackgroundFetchRegistration object that will be given to the
  // developer, representing the data associated with the |controller|.
  BackgroundFetchRegistration registration;
  registration.tag = controller->registration_id().tag();
  registration.icons = controller->options().icons;
  registration.title = controller->options().title;
  registration.total_download_size = controller->options().total_download_size;

  callback.Run(blink::mojom::BackgroundFetchError::NONE, registration);
}

void BackgroundFetchServiceImpl::GetTags(int64_t service_worker_registration_id,
                                         const url::Origin& origin,
                                         const GetTagsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(
      blink::mojom::BackgroundFetchError::NONE,
      background_fetch_context_->GetActiveTagsForServiceWorkerRegistration(
          service_worker_registration_id, origin));
}

bool BackgroundFetchServiceImpl::ValidateTag(const std::string& tag) {
  if (tag.empty() || tag.size() > kMaxTagLength) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::BFSI_INVALID_TAG);
    return false;
  }

  return true;
}

bool BackgroundFetchServiceImpl::ValidateRequests(
    const std::vector<ServiceWorkerFetchRequest>& requests) {
  if (requests.empty()) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::BFSI_INVALID_REQUESTS);
    return false;
  }

  return true;
}

bool BackgroundFetchServiceImpl::ValidateTitle(const std::string& title) {
  if (title.empty() || title.size() > kMaxTitleLength) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::BFSI_INVALID_TITLE);
    return false;
  }

  return true;
}

}  // namespace content
