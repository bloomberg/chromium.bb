// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/renderer/plugins/power_saver_info.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "url/origin.h"

namespace {

// Presence of the poster param within plugin object tags.
// These numeric values are used in UMA logs; do not change them.
enum PosterParamPresence {
  POSTER_PRESENCE_NO_PARAM_PPS_DISABLED = 0,
  POSTER_PRESENCE_NO_PARAM_PPS_ENABLED = 1,
  POSTER_PRESENCE_PARAM_EXISTS_PPS_DISABLED = 2,
  POSTER_PRESENCE_PARAM_EXISTS_PPS_ENABLED = 3,
  POSTER_PRESENCE_NUM_ITEMS
};

const char kPluginPowerSaverPosterParamPresenceHistogram[] =
    "Plugin.PowerSaver.PosterParamPresence";

void RecordPosterParamPresence(PosterParamPresence presence) {
  UMA_HISTOGRAM_ENUMERATION(kPluginPowerSaverPosterParamPresenceHistogram,
                            presence, POSTER_PRESENCE_NUM_ITEMS);
}

void TrackPosterParamPresence(const blink::WebPluginParams& params,
                              bool power_saver_enabled) {
  DCHECK_EQ(params.attributeNames.size(), params.attributeValues.size());

  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    if (params.attributeNames[i].utf8() == "poster") {
      RecordPosterParamPresence(
          power_saver_enabled ? POSTER_PRESENCE_PARAM_EXISTS_PPS_ENABLED
                              : POSTER_PRESENCE_PARAM_EXISTS_PPS_DISABLED);
      return;
    }
  }

  RecordPosterParamPresence(power_saver_enabled
                                ? POSTER_PRESENCE_NO_PARAM_PPS_ENABLED
                                : POSTER_PRESENCE_NO_PARAM_PPS_DISABLED);
}

std::string GetPluginInstancePosterAttribute(
    const blink::WebPluginParams& params) {
  DCHECK_EQ(params.attributeNames.size(), params.attributeValues.size());

  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    if (params.attributeNames[i].utf8() == "poster" &&
        !params.attributeValues[i].isEmpty()) {
      return params.attributeValues[i].utf8();
    }
  }
  return std::string();
}

}  // namespace

PowerSaverInfo::PowerSaverInfo()
    : power_saver_enabled(false), blocked_for_background_tab(false) {}

PowerSaverInfo::PowerSaverInfo(const PowerSaverInfo& other) = default;

PowerSaverInfo PowerSaverInfo::Get(content::RenderFrame* render_frame,
                                   bool power_saver_setting_on,
                                   const blink::WebPluginParams& params,
                                   const content::WebPluginInfo& plugin_info,
                                   const GURL& document_url) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string override_for_testing = command_line->GetSwitchValueASCII(
      switches::kOverridePluginPowerSaverForTesting);

  // This feature has only been tested thoroughly with Flash thus far.
  // It is also enabled for the Power Saver test plugin for browser tests.
  bool is_flash =
      plugin_info.name == base::ASCIIToUTF16(content::kFlashPluginName);

  PowerSaverInfo info;
  bool is_eligible = power_saver_setting_on && is_flash;
  info.power_saver_enabled = override_for_testing == "always" ||
                             (power_saver_setting_on && is_eligible);

  if (info.power_saver_enabled) {
    // Even if we disable PPS in the next block because content is same-origin,
    // it should still be eligible for background tab deferral if PPS is on.
    info.blocked_for_background_tab = render_frame->IsHidden();

    auto status = render_frame->GetPeripheralContentStatus(
        render_frame->GetWebFrame()->top()->getSecurityOrigin(),
        url::Origin(params.url), gfx::Size(),
        content::RenderFrame::RECORD_DECISION);

    // Early-exit from the whole Power Saver system if the content is
    // same-origin or whitelisted-origin. We ignore the other possibilities,
    // because we don't know the unobscured size of the plugin content yet.
    // If we are filtering same-origin tiny content, we cannot early exit here.
    //
    // Once the plugin is loaded, the peripheral content status is re-tested
    // with the actual unobscured plugin size.
    if (!base::FeatureList::IsEnabled(features::kFilterSameOriginTinyPlugin) &&
        (status == content::RenderFrame::CONTENT_STATUS_ESSENTIAL_SAME_ORIGIN ||
         status == content::RenderFrame::
                       CONTENT_STATUS_ESSENTIAL_CROSS_ORIGIN_WHITELISTED)) {
      info.power_saver_enabled = false;
    } else {
      info.poster_attribute = GetPluginInstancePosterAttribute(params);
      info.base_url = document_url;
    }
  }

  if (is_flash)
    TrackPosterParamPresence(params, info.power_saver_enabled);

  return info;
}
