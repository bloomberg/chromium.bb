// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/embedded_worker_start_params_struct_traits.h"

#include "content/public/common/common_param_traits_macros.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "url/mojo/url_gurl_struct_traits.h"

namespace mojo {

// static
bool StructTraits<content::mojom::EmbeddedWorkerStartParamsDataView,
                  content::EmbeddedWorkerStartParams>::
    Read(content::mojom::EmbeddedWorkerStartParamsDataView in,
         content::EmbeddedWorkerStartParams* out) {
  if (!in.ReadScope(&out->scope) || !in.ReadScriptUrl(&out->script_url) ||
      !in.ReadDevtoolsWorkerToken(&out->devtools_worker_token) ||
      !in.ReadV8CacheOptions(&out->settings.v8_cache_options)) {
    return false;
  }
  out->embedded_worker_id = in.embedded_worker_id();
  out->service_worker_version_id = in.service_worker_version_id();
  out->worker_devtools_agent_route_id = in.worker_devtools_agent_route_id();
  out->pause_after_download = in.pause_after_download();
  out->wait_for_debugger = in.wait_for_debugger();
  out->is_installed = in.is_installed();
  out->settings.data_saver_enabled = in.data_saver_enabled();
  return true;
}

}  // namespace mojo
