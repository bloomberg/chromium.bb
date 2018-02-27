// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_permission_manager.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"
#include "media/base/media_switches.h"

namespace content {

namespace {

bool IsWhitelistedPermissionType(PermissionType permission) {
  return permission == PermissionType::GEOLOCATION ||
         permission == PermissionType::MIDI ||
         permission == PermissionType::SENSORS ||
         permission == PermissionType::PAYMENT_HANDLER ||
         // Background sync browser tests require permission to be granted by
         // default.
         // TODO(nsatragno): add a command line flag so that it's only granted
         // for tests.
         permission == PermissionType::BACKGROUND_SYNC;
}

}  // namespace

ShellPermissionManager::ShellPermissionManager()
    : PermissionManager() {
}

ShellPermissionManager::~ShellPermissionManager() {
}

int ShellPermissionManager::RequestPermission(
    PermissionType permission,
    RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  callback.Run(IsWhitelistedPermissionType(permission)
                   ? blink::mojom::PermissionStatus::GRANTED
                   : blink::mojom::PermissionStatus::DENIED);
  return kNoPendingOperation;
}

int ShellPermissionManager::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<
        void(const std::vector<blink::mojom::PermissionStatus>&)>& callback) {
  std::vector<blink::mojom::PermissionStatus> result;
  for (const auto& permission : permissions) {
    result.push_back(IsWhitelistedPermissionType(permission)
                         ? blink::mojom::PermissionStatus::GRANTED
                         : blink::mojom::PermissionStatus::DENIED);
  }
  callback.Run(result);
  return kNoPendingOperation;
}

void ShellPermissionManager::ResetPermission(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

blink::mojom::PermissionStatus ShellPermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if ((permission == PermissionType::AUDIO_CAPTURE ||
       permission == PermissionType::VIDEO_CAPTURE) &&
      command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream) &&
      command_line->HasSwitch(switches::kUseFakeUIForMediaStream)) {
    return blink::mojom::PermissionStatus::GRANTED;
  }

  return IsWhitelistedPermissionType(permission)
             ? blink::mojom::PermissionStatus::GRANTED
             : blink::mojom::PermissionStatus::DENIED;
}

int ShellPermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(blink::mojom::PermissionStatus)>& callback) {
  return kNoPendingOperation;
}

void ShellPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
}

}  // namespace content
