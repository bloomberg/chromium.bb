// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/plugins/plugins_field_trial.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

FlashPermissionContext::FlashPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            content::PermissionType::FLASH,
                            CONTENT_SETTINGS_TYPE_PLUGINS) {}

FlashPermissionContext::~FlashPermissionContext() {}

ContentSetting FlashPermissionContext::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ContentSetting flash_setting = PluginUtils::GetFlashPluginContentSetting(
      host_content_settings_map, url::Origin(embedding_origin),
      requesting_origin, nullptr);
  flash_setting = PluginsFieldTrial::EffectiveContentSetting(
      host_content_settings_map, CONTENT_SETTINGS_TYPE_PLUGINS, flash_setting);
  if (flash_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT)
    return CONTENT_SETTING_ASK;
  return flash_setting;
}

void FlashPermissionContext::UpdateTabContext(const PermissionRequestID& id,
                                              const GURL& requesting_origin,
                                              bool allowed) {
  if (!allowed)
    return;
  // Automatically refresh the page.
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(id.render_process_id(),
                                           id.render_frame_id()));
  web_contents->GetController().Reload(true /* check_for_repost */);
}

void FlashPermissionContext::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    ContentSetting content_setting) {
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedding_origin, embedding_origin.GetOrigin());
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  // If the request was for a file scheme, allow or deny all file:/// URLs.
  ContentSettingsPattern pattern;
  if (embedding_origin.SchemeIsFile())
    pattern = ContentSettingsPattern::FromString("file:///*");
  else
    pattern = ContentSettingsPattern::FromURLNoWildcard(embedding_origin);

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetContentSettingCustomScope(
          pattern, ContentSettingsPattern::Wildcard(), content_settings_type(),
          std::string(), content_setting);
}

bool FlashPermissionContext::IsRestrictedToSecureOrigins() const {
  return false;
}
