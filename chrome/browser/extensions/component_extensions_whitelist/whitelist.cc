// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_extensions_whitelist/whitelist.h"

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"
#include "grit/browser_resources.h"

#if defined(ENABLE_APP_LIST) && defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/google_now_extension.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/component_extension_ime_manager_impl.h"
#include "components/chrome_apps/grit/chrome_apps_resources.h"
#include "grit/keyboard_resources.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#endif

namespace extensions {

bool IsComponentExtensionWhitelisted(const std::string& extension_id) {
  const char* allowed[] = {
    extension_misc::kHotwordSharedModuleId,
    extension_misc::kInAppPaymentsSupportAppId,
#if defined(ENABLE_MEDIA_ROUTER)
    extension_misc::kMediaRouterStableExtensionId,
#endif  // defined(ENABLE_MEDIA_ROUTER)
    extension_misc::kPdfExtensionId,
#if defined(OS_CHROMEOS)
    extension_misc::kChromeVoxExtensionId,
    extension_misc::kSpeechSynthesisExtensionId,
    extension_misc::kZIPUnpackerExtensionId,
#endif
  };

  for (size_t i = 0; i < arraysize(allowed); ++i) {
    if (extension_id == allowed[i])
      return true;
  }

#if defined(ENABLE_APP_LIST) && defined(OS_CHROMEOS)
  std::string google_now_extension_id;
  if (GetGoogleNowExtensionId(&google_now_extension_id) &&
      google_now_extension_id == extension_id)
    return true;
#endif

#if defined(OS_CHROMEOS)
  if (chromeos::ComponentExtensionIMEManagerImpl::IsIMEExtensionID(
          extension_id)) {
    return true;
  }
#endif
  LOG(ERROR) << "Component extension with id " << extension_id << " not in "
             << "whitelist and is not being loaded as a result.";
  NOTREACHED();
  return false;
}

bool IsComponentExtensionWhitelisted(int manifest_resource_id) {
  int allowed[] = {
    IDR_BOOKMARKS_MANIFEST,
    IDR_CHROME_APP_MANIFEST,
    IDR_CLOUDPRINT_MANIFEST,
    IDR_CONNECTIVITY_DIAGNOSTICS_MANIFEST,
    IDR_CRYPTOTOKEN_MANIFEST,
    IDR_FEEDBACK_MANIFEST,
    IDR_GAIA_AUTH_MANIFEST,
    IDR_GOOGLE_NOW_MANIFEST,
    IDR_HANGOUT_SERVICES_MANIFEST,
    IDR_HOTWORD_AUDIO_VERIFICATION_MANIFEST,
    IDR_HOTWORD_MANIFEST,
    IDR_IDENTITY_API_SCOPE_APPROVAL_MANIFEST,
    IDR_NETWORK_SPEECH_SYNTHESIS_MANIFEST,
    IDR_SETTINGS_APP_MANIFEST,
    IDR_WALLPAPERMANAGER_MANIFEST,
    IDR_WEBSTORE_MANIFEST,
    IDR_WHISPERNET_PROXY_MANIFEST,
#if defined(IMAGE_LOADER_EXTENSION)
    IDR_IMAGE_LOADER_MANIFEST,
#endif
#if defined(OS_CHROMEOS)
    IDR_AUDIO_PLAYER_MANIFEST,
    IDR_CHROME_APPS_WEBSTORE_WIDGET_MANIFEST,
    IDR_CONNECTIVITY_DIAGNOSTICS_LAUNCHER_MANIFEST,
    IDR_CONNECTIVITY_DIAGNOSTICS_MANIFEST,
    IDR_CROSH_BUILTIN_MANIFEST,
    IDR_DEMO_APP_MANIFEST,
    IDR_EASY_UNLOCK_MANIFEST,
    IDR_EASY_UNLOCK_MANIFEST_SIGNIN,
    IDR_ECHO_MANIFEST,
    IDR_FILEMANAGER_MANIFEST,
    IDR_FIRST_RUN_DIALOG_MANIFEST,
    IDR_GALLERY_MANIFEST,
    IDR_GENIUS_APP_MANIFEST,
    IDR_HELP_MANIFEST,
    IDR_KEYBOARD_MANIFEST,
    IDR_MOBILE_MANIFEST,
    IDR_QUICKOFFICE_MANIFEST,
    IDR_VIDEO_PLAYER_MANIFEST,
    IDR_WALLPAPERMANAGER_MANIFEST,
#endif
  };

  for (size_t i = 0; i < arraysize(allowed); ++i) {
    if (manifest_resource_id == allowed[i])
      return true;
  }

  LOG(ERROR) << "Component extension with manifest resource id "
             << manifest_resource_id << " not in whitelist and is not being "
             << "loaded as a result.";
  NOTREACHED();
  return false;
}

}  // namespace extensions
