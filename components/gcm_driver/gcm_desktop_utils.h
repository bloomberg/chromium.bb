// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_GCM_DESKTOP_UTILS_H_
#define COMPONENTS_GCM_DRIVER_GCM_GCM_DESKTOP_UTILS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "components/version_info/version_info.h"

class PrefService;
namespace base {
class FilePath;
}

namespace net {
class URLRequestContextGetter;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace gcm {

class GCMDriver;
class GCMClientFactory;

std::unique_ptr<GCMDriver> CreateGCMDriverDesktop(
    std::unique_ptr<GCMClientFactory> gcm_client_factory,
    PrefService* prefs,
    const base::FilePath& store_path,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    version_info::Channel channel,
    const std::string& product_category_for_subtypes,
    const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_GCM_DESKTOP_UTILS_H_
