// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_content_settings_proxy_impl.h"

#include <utility>

#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/gurl.h"

namespace content {

void SharedWorkerContentSettingsProxyImpl::Create(
    base::WeakPtr<SharedWorkerHost> host,
    blink::mojom::SharedWorkerContentSettingsProxyRequest request) {
  mojo::MakeStrongBinding(
      base::WrapUnique(new SharedWorkerContentSettingsProxyImpl(host)),
      std::move(request));
}

SharedWorkerContentSettingsProxyImpl::SharedWorkerContentSettingsProxyImpl(
    base::WeakPtr<SharedWorkerHost> host)
    : host_(std::move(host)) {}

SharedWorkerContentSettingsProxyImpl::~SharedWorkerContentSettingsProxyImpl() =
    default;

void SharedWorkerContentSettingsProxyImpl::AllowIndexedDB(
    const url::Origin& origin,
    const base::string16& name,
    AllowIndexedDBCallback callback) {
  if (!origin.unique() && host_) {
    bool result = host_->AllowIndexedDB(origin.GetURL(), name);
    std::move(callback).Run(result);
  } else
    std::move(callback).Run(false);
}

void SharedWorkerContentSettingsProxyImpl::RequestFileSystemAccessSync(
    const url::Origin& origin,
    RequestFileSystemAccessSyncCallback callback) {
  if (!origin.unique() && host_) {
    host_->AllowFileSystem(origin.GetURL(), std::move(callback));
  } else
    std::move(callback).Run(false);
}

}  // namespace content
