// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_service_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

// static
void BackgroundFetchServiceImpl::Create(
    scoped_refptr<BackgroundFetchContext> background_fetch_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    blink::mojom::BackgroundFetchServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  mojo::MakeStrongBinding(base::MakeUnique<BackgroundFetchServiceImpl>(
                              std::move(background_fetch_context),
                              std::move(service_worker_context)),
                          std::move(request));
}

BackgroundFetchServiceImpl::BackgroundFetchServiceImpl(
    scoped_refptr<BackgroundFetchContext> background_fetch_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : background_fetch_context_(std::move(background_fetch_context)),
      service_worker_context_(std::move(service_worker_context)) {
  DCHECK(background_fetch_context_);
  DCHECK(service_worker_context_);
}

BackgroundFetchServiceImpl::~BackgroundFetchServiceImpl() = default;

void BackgroundFetchServiceImpl::UpdateUI(
    int64_t service_worker_registration_id,
    const std::string& tag,
    const std::string& title,
    const UpdateUICallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(peter): Get the BackgroundFetchJobController for the
  // {service_worker_registration_id, tag} pair and call UpdateUI() on it.

  callback.Run(blink::mojom::BackgroundFetchError::NONE);
}

void BackgroundFetchServiceImpl::Abort(int64_t service_worker_registration_id,
                                       const std::string& tag,
                                       const AbortCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(peter): Get the BackgroundFetchJobController for the
  // {service_worker_registration_id, tag} pair and call Abort() on it.

  callback.Run(blink::mojom::BackgroundFetchError::NONE);
}

void BackgroundFetchServiceImpl::GetRegistration(
    int64_t service_worker_registration_id,
    const std::string& tag,
    const GetRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(peter): Get the registration for {service_worker_registration_id, tag}
  // and construct a BackgroundFetchRegistrationPtr for it.

  callback.Run(blink::mojom::BackgroundFetchError::NONE,
               nullptr /* registration */);
}

void BackgroundFetchServiceImpl::GetTags(int64_t service_worker_registration_id,
                                         const GetTagsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(peter): Get the list of active Background Fetches associated with
  // service_worker_registration_id and share their tags.

  callback.Run(blink::mojom::BackgroundFetchError::NONE,
               std::vector<std::string>());
}

}  // namespace content
