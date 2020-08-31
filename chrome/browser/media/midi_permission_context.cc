// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_permission_context.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

MidiPermissionContext::MidiPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::MIDI,
                            blink::mojom::FeaturePolicyFeature::kMidiFeature) {}

MidiPermissionContext::~MidiPermissionContext() {
}

ContentSetting MidiPermissionContext::GetPermissionStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  return CONTENT_SETTING_ALLOW;
}

bool MidiPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
