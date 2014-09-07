// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_utils.h"

#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#endif

class Profile;

namespace content {
class WebContents;
}

void RequestMediaAccessPermission(
    content::WebContents* web_contents,
    Profile* profile,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  const extensions::Extension* extension = NULL;
#if defined(ENABLE_EXTENSIONS)
  GURL origin(request.security_origin);
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    ExtensionService* extensions_service =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    extension = extensions_service->extensions()->GetByID(origin.host());
    DCHECK(extension);
  }
#endif

  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, extension);
}
