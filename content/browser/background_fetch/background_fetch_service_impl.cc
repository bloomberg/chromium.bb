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

// Maximum length of a developer-provided id for a Background Fetch.
constexpr size_t kMaxIdLength = 1024 * 1024;

// Maximum length of a developer-provided title for a Background Fetch.
constexpr size_t kMaxTitleLength = 1024 * 1024;

}  // namespace

// static
void BackgroundFetchServiceImpl::Create(
    int render_process_id,
    scoped_refptr<BackgroundFetchContext> background_fetch_context,
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

BackgroundFetchServiceImpl::~BackgroundFetchServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void BackgroundFetchServiceImpl::Fetch(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateId(id)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
        base::nullopt /* registration */);
    return;
  }

  if (!ValidateRequests(requests)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
        base::nullopt /* registration */);
    return;
  }

  BackgroundFetchRegistrationId registration_id(service_worker_registration_id,
                                                origin, id);

  background_fetch_context_->StartFetch(registration_id, requests, options,
                                        std::move(callback));
}

void BackgroundFetchServiceImpl::UpdateUI(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& id,
    const std::string& title,
    UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateId(id) || !ValidateTitle(title)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  BackgroundFetchJobController* controller =
      background_fetch_context_->GetActiveFetch(BackgroundFetchRegistrationId(
          service_worker_registration_id, origin, id));

  if (controller)
    controller->UpdateUI(title);

  std::move(callback).Run(controller
                              ? blink::mojom::BackgroundFetchError::NONE
                              : blink::mojom::BackgroundFetchError::INVALID_ID);
}

void BackgroundFetchServiceImpl::Abort(int64_t service_worker_registration_id,
                                       const url::Origin& origin,
                                       const std::string& id,
                                       AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateId(id)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  BackgroundFetchJobController* controller =
      background_fetch_context_->GetActiveFetch(BackgroundFetchRegistrationId(
          service_worker_registration_id, origin, id));

  if (controller)
    controller->Abort();

  std::move(callback).Run(controller
                              ? blink::mojom::BackgroundFetchError::NONE
                              : blink::mojom::BackgroundFetchError::INVALID_ID);
}

void BackgroundFetchServiceImpl::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& id,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateId(id)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
        base::nullopt /* registration */);
    return;
  }

  BackgroundFetchJobController* controller =
      background_fetch_context_->GetActiveFetch(BackgroundFetchRegistrationId(
          service_worker_registration_id, origin, id));

  if (!controller) {
    std::move(callback).Run(blink::mojom::BackgroundFetchError::INVALID_ID,
                            base::nullopt /* registration */);
    return;
  }

  // Compile the BackgroundFetchRegistration object that will be given to the
  // developer, representing the data associated with the |controller|.
  BackgroundFetchRegistration registration;
  registration.id = controller->registration_id().id();
  registration.icons = controller->options().icons;
  registration.title = controller->options().title;
  registration.total_download_size = controller->options().total_download_size;

  std::move(callback).Run(blink::mojom::BackgroundFetchError::NONE,
                          registration);
}

void BackgroundFetchServiceImpl::GetIds(int64_t service_worker_registration_id,
                                        const url::Origin& origin,
                                        GetIdsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(callback).Run(
      blink::mojom::BackgroundFetchError::NONE,
      background_fetch_context_->GetActiveIdsForServiceWorkerRegistration(
          service_worker_registration_id, origin));
}

bool BackgroundFetchServiceImpl::ValidateId(const std::string& id) {
  if (id.empty() || id.size() > kMaxIdLength) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::BFSI_INVALID_ID);
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
