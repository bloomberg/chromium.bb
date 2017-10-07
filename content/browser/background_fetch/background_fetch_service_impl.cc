// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_service_impl.h"

#include "base/guid.h"
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

// Maximum length of a developer-provided |developer_id| for a Background Fetch.
constexpr size_t kMaxDeveloperIdLength = 1024 * 1024;

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
    const std::string& developer_id,
    const std::vector<ServiceWorkerFetchRequest>& requests,
    const BackgroundFetchOptions& options,
    FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateDeveloperId(developer_id)) {
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

  // New |unique_id|, since this is a new Background Fetch registration. This is
  // the only place new |unique_id|s should be created outside of tests.
  BackgroundFetchRegistrationId registration_id(service_worker_registration_id,
                                                origin, developer_id,
                                                base::GenerateGUID());

  background_fetch_context_->StartFetch(registration_id, requests, options,
                                        std::move(callback));
}

void BackgroundFetchServiceImpl::UpdateUI(const std::string& unique_id,
                                          const std::string& title,
                                          UpdateUICallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateUniqueId(unique_id) || !ValidateTitle(title)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  background_fetch_context_->data_manager().UpdateRegistrationUI(
      unique_id, title, std::move(callback));
}

void BackgroundFetchServiceImpl::Abort(const std::string& unique_id,
                                       AbortCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateUniqueId(unique_id)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT);
    return;
  }

  BackgroundFetchJobController* controller =
      background_fetch_context_->GetActiveFetch(unique_id);

  if (controller)
    controller->Abort();

  std::move(callback).Run(controller
                              ? blink::mojom::BackgroundFetchError::NONE
                              : blink::mojom::BackgroundFetchError::INVALID_ID);
}

void BackgroundFetchServiceImpl::GetRegistration(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& developer_id,
    GetRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!ValidateDeveloperId(developer_id)) {
    std::move(callback).Run(
        blink::mojom::BackgroundFetchError::INVALID_ARGUMENT,
        base::nullopt /* registration */);
    return;
  }

  background_fetch_context_->data_manager().GetRegistration(
      service_worker_registration_id, origin, developer_id,
      std::move(callback));
}

void BackgroundFetchServiceImpl::GetDeveloperIds(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    GetDeveloperIdsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  background_fetch_context_->data_manager().GetDeveloperIdsForServiceWorker(
      service_worker_registration_id, std::move(callback));
}

bool BackgroundFetchServiceImpl::ValidateDeveloperId(
    const std::string& developer_id) {
  if (developer_id.empty() || developer_id.size() > kMaxDeveloperIdLength) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::BFSI_INVALID_DEVELOPER_ID);
    return false;
  }

  return true;
}

bool BackgroundFetchServiceImpl::ValidateUniqueId(
    const std::string& unique_id) {
  if (!base::IsValidGUID(unique_id)) {
    bad_message::ReceivedBadMessage(render_process_id_,
                                    bad_message::BFSI_INVALID_UNIQUE_ID);
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
