// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_device_permissions.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

namespace {

bool IsInKioskMode() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return true;

#if defined(OS_CHROMEOS)
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  return user_manager && user_manager->IsLoggedInAsKioskApp();
#else
  return false;
#endif
}

}  // namespace

bool ShouldPersistContentSetting(ContentSetting setting,
                                 const GURL& origin,
                                 content::MediaStreamRequestType type) {
  // When the request is from an invalid scheme we don't persist it.
  if (!ContentSettingsPattern::FromURLNoWildcard(origin).IsValid())
    return false;

  // It's safe to persist block settings all the time.
  if (setting == CONTENT_SETTING_BLOCK)
    return true;

  // Pepper requests should always be persisted to prevent annoying users of
  // plugins.
  if (type == content::MEDIA_OPEN_DEVICE)
    return true;

  // We persist requests from secure origins.
  if (origin.SchemeIsSecure())
    return true;

  // We persist requests from extensions.
  if (origin.SchemeIs(extensions::kExtensionScheme))
    return true;

  return false;
}

bool CheckAllowAllMediaStreamContentForOrigin(Profile* profile,
                                              const GURL& security_origin,
                                              ContentSettingsType type) {
  DCHECK(type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
         type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  return profile->GetHostContentSettingsMap()->ShouldAllowAllContent(
      security_origin, security_origin, type);
}

MediaStreamDevicePolicy GetDevicePolicy(const Profile* profile,
                                        const GURL& security_origin,
                                        const char* policy_name,
                                        const char* whitelist_policy_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the security origin policy matches a value in the whitelist, allow it.
  // Otherwise, check the |policy_name| master switch for the default behavior.

  const PrefService* prefs = profile->GetPrefs();

  // TODO(tommi): Remove the kiosk mode check when the whitelist below
  // is visible in the media exceptions UI.
  // See discussion here: https://codereview.chromium.org/15738004/
  if (IsInKioskMode()) {
    const base::ListValue* list = prefs->GetList(whitelist_policy_name);
    std::string value;
    for (size_t i = 0; i < list->GetSize(); ++i) {
      if (list->GetString(i, &value)) {
        ContentSettingsPattern pattern =
            ContentSettingsPattern::FromString(value);
        if (pattern == ContentSettingsPattern::Wildcard()) {
          DLOG(WARNING) << "Ignoring wildcard URL pattern: " << value;
          continue;
        }
        DLOG_IF(ERROR, !pattern.IsValid()) << "Invalid URL pattern: " << value;
        if (pattern.IsValid() && pattern.Matches(security_origin))
          return ALWAYS_ALLOW;
      }
    }
  }

  // If a match was not found, check if audio capture is otherwise disallowed
  // or if the user should be prompted.  Setting the policy value to "true"
  // is equal to not setting it at all, so from hereon out, we will return
  // either POLICY_NOT_SET (prompt) or ALWAYS_DENY (no prompt, no access).
  if (!prefs->GetBoolean(policy_name))
    return ALWAYS_DENY;

  return POLICY_NOT_SET;
}
