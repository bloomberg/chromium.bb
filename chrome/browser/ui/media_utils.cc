// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_utils.h"

#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#endif

namespace {

#if defined(ENABLE_EXTENSIONS)
const extensions::Extension* GetExtensionForOrigin(Profile* profile,
                                             const GURL& security_origin) {
  if (!security_origin.SchemeIs(extensions::kExtensionScheme))
    return NULL;

  ExtensionService* extensions_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const extensions::Extension* extension =
      extensions_service->extensions()->GetByID(security_origin.host());
  DCHECK(extension);
  return extension;
}
#endif

}  // namespace

void RequestMediaAccessPermission(
    content::WebContents* web_contents,
    Profile* profile,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  const extensions::Extension* extension = NULL;
#if defined(ENABLE_EXTENSIONS)
  extension = GetExtensionForOrigin(profile, request.security_origin);
#endif
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, callback, extension);
}

bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                const GURL& security_origin,
                                content::MediaStreamType type) {
#if defined(ENABLE_EXTENSIONS)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const extensions::Extension* extension =
      GetExtensionForOrigin(profile, security_origin);
  if (extension) {
    return MediaCaptureDevicesDispatcher::GetInstance()
        ->CheckMediaAccessPermission(
            web_contents, security_origin, type, extension);
  }
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(web_contents, security_origin, type);
#else
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(web_contents, security_origin, type);
#endif
}
