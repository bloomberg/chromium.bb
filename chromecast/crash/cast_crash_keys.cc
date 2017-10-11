// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/cast_crash_keys.h"

#include "components/crash/core/common/crash_keys.h"
#include "gpu/config/gpu_crash_keys.h"

namespace chromecast {
namespace crash_keys {

const char kLastApp[] = "last_app";
const char kCurrentApp[] = "current_app";
const char kPreviousApp[] = "previous_app";

size_t RegisterCastCrashKeys() {
  const base::debug::CrashKey fixed_keys[] = {
      {kLastApp, ::crash_keys::kSmallSize},
      {kCurrentApp, ::crash_keys::kSmallSize},
      {kPreviousApp, ::crash_keys::kSmallSize},

      // TODO(sanfin): The following crash keys are copied from
      // chrome/common/crash_keys.cc. When http://crbug.com/598854 is fixed,
      // remove these and refactor as necessary.

      {::crash_keys::kClientId, ::crash_keys::kSmallSize},
      {::crash_keys::kChannel, ::crash_keys::kSmallSize},
      {"url-chunk", ::crash_keys::kLargeSize},
      {::crash_keys::kNumVariations, ::crash_keys::kSmallSize},
      {::crash_keys::kVariations, ::crash_keys::kHugeSize},
      {"num-extensions", ::crash_keys::kSmallSize},
      {"shutdown-type", ::crash_keys::kSmallSize},
      {"browser-unpin-trace", ::crash_keys::kMediumSize},

      // gpu
      {gpu::crash_keys::kGPUDriverVersion, ::crash_keys::kSmallSize},
      {gpu::crash_keys::kGPUPixelShaderVersion, ::crash_keys::kSmallSize},
      {gpu::crash_keys::kGPUVertexShaderVersion, ::crash_keys::kSmallSize},
      {gpu::crash_keys::kGPUGLContextIsVirtual, ::crash_keys::kSmallSize},

      // content/:
      {"bad_message_reason", ::crash_keys::kSmallSize},
      {"discardable-memory-allocated", ::crash_keys::kSmallSize},
      {"discardable-memory-free", ::crash_keys::kSmallSize},
      {"font_key_name", ::crash_keys::kSmallSize},
      {"mojo-message-error", ::crash_keys::kMediumSize},
      {"ppapi_path", ::crash_keys::kMediumSize},
      {"subresource_url", ::crash_keys::kLargeSize},
      {"total-discardable-memory-allocated", ::crash_keys::kSmallSize},
      {"input-event-filter-send-failure", ::crash_keys::kSmallSize},
      // media/:
      {::crash_keys::kBug464926CrashKey, ::crash_keys::kSmallSize},
      {"view-count", ::crash_keys::kSmallSize},

      // media/:
      {"zero-encode-details", ::crash_keys::kSmallSize},

      // gin/:
      {"v8-ignition", ::crash_keys::kSmallSize},

      // Site isolation.  These keys help debug renderer kills such as
      // https://crbug.com/773140.
      {"requested_site_url", ::crash_keys::kSmallSize},
      {"requested_origin", ::crash_keys::kSmallSize},
      {"killed_process_origin_lock", ::crash_keys::kSmallSize},
      {"site_isolation_mode", ::crash_keys::kSmallSize},

      // Temporary for https://crbug.com/626802.
      {"newframe_routing_id", ::crash_keys::kSmallSize},
      {"newframe_proxy_id", ::crash_keys::kSmallSize},
      {"newframe_opener_id", ::crash_keys::kSmallSize},
      {"newframe_parent_id", ::crash_keys::kSmallSize},
      {"newframe_widget_id", ::crash_keys::kSmallSize},
      {"newframe_widget_hidden", ::crash_keys::kSmallSize},
      {"newframe_replicated_origin", ::crash_keys::kSmallSize},

      // Temporary for https://crbug.com/612711.
      {"aci_wrong_sp_extension_id", ::crash_keys::kSmallSize},

      // Temporary for https://crbug.com/685996.
      {"user-cloud-policy-manager-connect-trace", ::crash_keys::kMediumSize},
  };

  return base::debug::InitCrashKeys(fixed_keys, arraysize(fixed_keys),
                                    ::crash_keys::kChunkMaxLength);
}

}  // namespace crash_keys
}  // namespace chromecast
