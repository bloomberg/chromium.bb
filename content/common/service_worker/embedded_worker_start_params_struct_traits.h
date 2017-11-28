// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_START_PARAMS_STRUCT_TRAITS_H_
#define CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_START_PARAMS_STRUCT_TRAITS_H_

#include "content/common/service_worker/embedded_worker.mojom.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::EmbeddedWorkerStartParamsDataView,
                    content::EmbeddedWorkerStartParams> {
  static int embedded_worker_id(
      const content::EmbeddedWorkerStartParams& info) {
    return info.embedded_worker_id;
  }
  static int64_t service_worker_version_id(
      const content::EmbeddedWorkerStartParams& info) {
    return info.service_worker_version_id;
  }
  static const GURL& scope(const content::EmbeddedWorkerStartParams& info) {
    return info.scope;
  }
  static const GURL& script_url(
      const content::EmbeddedWorkerStartParams& info) {
    return info.script_url;
  }
  static int worker_devtools_agent_route_id(
      const content::EmbeddedWorkerStartParams& info) {
    return info.worker_devtools_agent_route_id;
  }
  static const base::UnguessableToken& devtools_worker_token(
      const content::EmbeddedWorkerStartParams& info) {
    return info.devtools_worker_token;
  }
  static bool pause_after_download(
      const content::EmbeddedWorkerStartParams& info) {
    return info.pause_after_download;
  }
  static bool wait_for_debugger(
      const content::EmbeddedWorkerStartParams& info) {
    return info.wait_for_debugger;
  }
  static bool is_installed(const content::EmbeddedWorkerStartParams& info) {
    return info.is_installed;
  }
  static content::V8CacheOptions v8_cache_options(
      const content::EmbeddedWorkerStartParams& info) {
    return info.settings.v8_cache_options;
  }
  static bool data_saver_enabled(
      const content::EmbeddedWorkerStartParams& info) {
    return info.settings.data_saver_enabled;
  }

  static bool Read(content::mojom::EmbeddedWorkerStartParamsDataView in,
                   content::EmbeddedWorkerStartParams* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_SERVICE_WORKER_EMBEDDED_WORKER_START_PARAMS_STRUCT_TRAITS_H_
