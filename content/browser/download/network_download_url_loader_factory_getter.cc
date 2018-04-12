// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/network_download_url_loader_factory_getter.h"

#include "components/download/public/common/download_task_runner.h"
#include "content/browser/url_loader_factory_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

NetworkDownloadURLLoaderFactoryGetter::NetworkDownloadURLLoaderFactoryGetter(
    scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter)
    : url_loader_factory_getter_(url_loader_factory_getter) {}

NetworkDownloadURLLoaderFactoryGetter::
    ~NetworkDownloadURLLoaderFactoryGetter() = default;

scoped_refptr<network::SharedURLLoaderFactory>
NetworkDownloadURLLoaderFactoryGetter::GetURLLoaderFactory() {
  DCHECK(download::GetIOTaskRunner());
  DCHECK(download::GetIOTaskRunner()->BelongsToCurrentThread());
  return url_loader_factory_getter_->GetNetworkFactory();
}

}  // namespace content
