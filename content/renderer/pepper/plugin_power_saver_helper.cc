// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/plugin_power_saver_helper.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/frame_messages.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

namespace {

// Initial decision of the peripheral content decision.
// These numeric values are used in UMA logs; do not change them.
enum PeripheralHeuristicDecision {
  HEURISTIC_DECISION_PERIPHERAL = 0,
  HEURISTIC_DECISION_ESSENTIAL_SAME_ORIGIN = 1,
  HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_BIG = 2,
  HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_WHITELISTED = 3,
  HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_TINY = 4,
  HEURISTIC_DECISION_NUM_ITEMS
};

const char kPeripheralHeuristicHistogram[] =
    "Plugin.PowerSaver.PeripheralHeuristic";

// Maximum dimensions plugin content may have while still being considered
// peripheral content. These match the sizes used by Safari.
const int kPeripheralContentMaxWidth = 400;
const int kPeripheralContentMaxHeight = 300;

// Plugin content below this size in height and width is considered "tiny".
// Tiny content is never peripheral, as tiny plugins often serve a critical
// purpose, and the user often cannot find and click to unthrottle it.
const int kPeripheralContentTinySize = 5;

void RecordDecisionMetric(PeripheralHeuristicDecision decision) {
  UMA_HISTOGRAM_ENUMERATION(kPeripheralHeuristicHistogram, decision,
                            HEURISTIC_DECISION_NUM_ITEMS);
}

}  // namespace

PluginPowerSaverHelper::PeripheralPlugin::PeripheralPlugin(
    const GURL& content_origin,
    const base::Closure& unthrottle_callback)
    : content_origin(content_origin), unthrottle_callback(unthrottle_callback) {
}

PluginPowerSaverHelper::PeripheralPlugin::~PeripheralPlugin() {
}

PluginPowerSaverHelper::PluginPowerSaverHelper(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

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
    const std::set<GURL>& origin_whitelist) {
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
    const GURL& content_origin,
    const base::Closure& unthrottle_callback) {
  DCHECK_EQ(content_origin.GetOrigin(), content_origin);
  peripheral_plugins_.push_back(
      PeripheralPlugin(content_origin, unthrottle_callback));
}

bool PluginPowerSaverHelper::ShouldThrottleContent(
    const GURL& content_origin,
    const std::string& plugin_module_name,
    int width,
    int height,
    bool* cross_origin_main_content) const {
  DCHECK_EQ(content_origin.GetOrigin(), content_origin);
  if (cross_origin_main_content)
    *cross_origin_main_content = false;

  // This feature has only been tested throughly with Flash thus far.
  if (plugin_module_name != content::kFlashPluginName)
    return false;

  if (width <= 0 || height <= 0)
    return false;

  // TODO(alexmos): Update this to use the origin of the RemoteFrame when 426512
  // is fixed. For now, case 3 in the class level comment doesn't work in
  // --site-per-process mode.
  blink::WebFrame* main_frame =
      render_frame()->GetWebFrame()->view()->mainFrame();
  if (main_frame->isWebRemoteFrame()) {
    RecordDecisionMetric(HEURISTIC_DECISION_PERIPHERAL);
    return true;
  }

  // All same-origin plugin content is essential.
  GURL main_frame_origin = GURL(main_frame->document().url()).GetOrigin();
  if (content_origin == main_frame_origin) {
    RecordDecisionMetric(HEURISTIC_DECISION_ESSENTIAL_SAME_ORIGIN);
    return false;
  }

  // Whitelisted plugin origins are also essential.
  if (origin_whitelist_.count(content_origin)) {
    RecordDecisionMetric(HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_WHITELISTED);
    return false;
  }

  // Never mark tiny content as peripheral.
  if (width <= kPeripheralContentTinySize &&
      height <= kPeripheralContentTinySize) {
    RecordDecisionMetric(HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_TINY);
    return false;
  }

  // Plugin content large in both dimensions are the "main attraction".
  if (width >= kPeripheralContentMaxWidth &&
      height >= kPeripheralContentMaxHeight) {
    RecordDecisionMetric(HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_BIG);
    if (cross_origin_main_content)
      *cross_origin_main_content = true;
    return false;
  }

  RecordDecisionMetric(HEURISTIC_DECISION_PERIPHERAL);
  return true;
}

void PluginPowerSaverHelper::WhitelistContentOrigin(
    const GURL& content_origin) {
  DCHECK_EQ(content_origin.GetOrigin(), content_origin);
  if (origin_whitelist_.insert(content_origin).second) {
    Send(new FrameHostMsg_PluginContentOriginAllowed(
        render_frame()->GetRoutingID(), content_origin));
  }
}

}  // namespace content
