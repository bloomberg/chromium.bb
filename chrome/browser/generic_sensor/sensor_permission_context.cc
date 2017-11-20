// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/generic_sensor/sensor_permission_context.h"

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/WebKit/common/feature_policy/feature_policy_feature.h"
#include "url/gurl.h"

SensorPermissionContext::SensorPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            CONTENT_SETTINGS_TYPE_SENSORS,
                            blink::FeaturePolicyFeature::kNotFound) {}

SensorPermissionContext::~SensorPermissionContext() {}

ContentSetting SensorPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  // TODO(juncai): We may need to add cross-origin iframes check here when we
  // can grant permission for certain sensor types. Currently this function
  // doesn't have any information of which sensor type requests permission.
  // The Generic Sensor API is not allowed in cross-origin iframes and
  // this is enforced by the renderer.
  // https://crbug.com/787019

  // This is to allow DeviceMotion and DeviceOrientation Event to be
  // able to access sensors (which are provided by generic sensor) in
  // cross-origin iframes. The Generic Sensor API is not allowed in
  // cross-origin iframes and this is enforced by the renderer.
  return CONTENT_SETTING_ALLOW;
}

bool SensorPermissionContext::IsRestrictedToSecureOrigins() const {
  // This is to allow non-secure origins that use DeviceMotion and
  // DeviceOrientation Event to be able to access sensors that are provided
  // by generic_sensor. The Generic Sensor API is not allowed in non-secure
  // origins and this is enforced by the renderer.
  return false;
}
