// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_registration_notifier.h"

#include "base/bind.h"
#include "base/stl_util.h"

namespace content {

BackgroundFetchRegistrationNotifier::BackgroundFetchRegistrationNotifier() =
    default;

BackgroundFetchRegistrationNotifier::~BackgroundFetchRegistrationNotifier() =
    default;

void BackgroundFetchRegistrationNotifier::AddObserver(
    const std::string& unique_id,
    blink::mojom::BackgroundFetchRegistrationObserverPtr observer) {
  // Observe connection errors, which occur when the JavaScript object or the
  // renderer hosting them goes away. (For example through navigation.) The
  // observer gets freed together with |this|, thus the Unretained is safe.
  observer.set_connection_error_handler(
      base::BindOnce(&BackgroundFetchRegistrationNotifier::OnConnectionError,
                     base::Unretained(this), unique_id, observer.get()));

  observers_.emplace(unique_id, std::move(observer));
}

void BackgroundFetchRegistrationNotifier::Notify(const std::string& unique_id,
                                                 uint64_t upload_total,
                                                 uint64_t uploaded,
                                                 uint64_t download_total,
                                                 uint64_t downloaded) {
  auto range = observers_.equal_range(unique_id);
  for (auto it = range.first; it != range.second; ++it)
    it->second->OnProgress(upload_total, uploaded, download_total, downloaded);
}

void BackgroundFetchRegistrationNotifier::RemoveObservers(
    const std::string& unique_id) {
  observers_.erase(unique_id);
}

void BackgroundFetchRegistrationNotifier::OnConnectionError(
    const std::string& unique_id,
    blink::mojom::BackgroundFetchRegistrationObserver* observer) {
  DCHECK_GE(observers_.count(unique_id), 1u);
  base::EraseIf(observers_,
                [observer](const auto& unique_id_observer_ptr_pair) {
                  return unique_id_observer_ptr_pair.second.get() == observer;
                });
}

}  // namespace content
