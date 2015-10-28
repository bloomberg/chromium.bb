// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/plugin_power_saver_helper.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/frame_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/peripheral_content_heuristic.h"
#include "ppapi/shared_impl/ppapi_constants.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

namespace {

const char kPeripheralHeuristicHistogram[] =
    "Plugin.PowerSaver.PeripheralHeuristic";

}  // namespace

PluginPowerSaverHelper::PeripheralPlugin::PeripheralPlugin(
    const url::Origin& content_origin,
    const base::Closure& unthrottle_callback)
    : content_origin(content_origin),
      unthrottle_callback(unthrottle_callback) {}

PluginPowerSaverHelper::PeripheralPlugin::~PeripheralPlugin() {
}

PluginPowerSaverHelper::PluginPowerSaverHelper(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

PluginPowerSaverHelper::~PluginPowerSaverHelper() {
}

void PluginPowerSaverHelper::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  // Only apply to top-level and new page navigation.
  if (frame->parent() || is_same_page_navigation)
    return;  // Not a top-level navigation.

  origin_whitelist_.clear();
}

bool PluginPowerSaverHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginPowerSaverHelper, message)
  IPC_MESSAGE_HANDLER(FrameMsg_UpdatePluginContentOriginWhitelist,
                      OnUpdatePluginContentOriginWhitelist)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PluginPowerSaverHelper::OnUpdatePluginContentOriginWhitelist(
    const std::set<url::Origin>& origin_whitelist) {
  origin_whitelist_ = origin_whitelist;

  // Check throttled plugin instances to see if any can be unthrottled.
  auto it = peripheral_plugins_.begin();
  while (it != peripheral_plugins_.end()) {
    if (origin_whitelist.count(it->content_origin)) {
      it->unthrottle_callback.Run();
      it = peripheral_plugins_.erase(it);
    } else {
      ++it;
    }
  }
}

void PluginPowerSaverHelper::RegisterPeripheralPlugin(
    const url::Origin& content_origin,
    const base::Closure& unthrottle_callback) {
  peripheral_plugins_.push_back(
      PeripheralPlugin(content_origin, unthrottle_callback));
}

bool PluginPowerSaverHelper::ShouldThrottleContent(
    const url::Origin& main_frame_origin,
    const url::Origin& content_origin,
    int width,
    int height,
    bool* cross_origin_main_content) const {
  if (cross_origin_main_content)
    *cross_origin_main_content = false;

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kOverridePluginPowerSaverForTesting) == "always") {
    return true;
  }

  auto decision = PeripheralContentHeuristic::GetPeripheralStatus(
      origin_whitelist_, main_frame_origin, content_origin, width, height);

  UMA_HISTOGRAM_ENUMERATION(
      kPeripheralHeuristicHistogram, decision,
      PeripheralContentHeuristic::HEURISTIC_DECISION_NUM_ITEMS);

  if (decision == PeripheralContentHeuristic::
                      HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_BIG &&
      cross_origin_main_content)
    *cross_origin_main_content = true;

  return decision == PeripheralContentHeuristic::HEURISTIC_DECISION_PERIPHERAL;
}

void PluginPowerSaverHelper::WhitelistContentOrigin(
    const url::Origin& content_origin) {
  if (origin_whitelist_.insert(content_origin).second) {
    Send(new FrameHostMsg_PluginContentOriginAllowed(
        render_frame()->GetRoutingID(), content_origin));
  }
}

}  // namespace content
