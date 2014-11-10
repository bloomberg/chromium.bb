// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "content/common/frame_messages.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/pepper/plugin_power_saver_helper.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
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
  HEURISTIC_DECISION_NUM_ITEMS
};

const char kPeripheralHeuristicHistogram[] =
    "Plugin.PowerSaver.PeripheralHeuristic";

// Maximum dimensions plug-in content may have while still being considered
// peripheral content. These match the sizes used by Safari.
const int kPeripheralContentMaxWidth = 400;
const int kPeripheralContentMaxHeight = 300;

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

void PluginPowerSaverHelper::DidCommitProvisionalLoad(bool is_new_navigation) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->parent())
    return;  // Not a top-level navigation.

  DocumentState* document_state =
      DocumentState::FromDataSource(frame->dataSource());
  NavigationState* navigation_state = document_state->navigation_state();
  if (!navigation_state->was_within_same_page())
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

bool PluginPowerSaverHelper::ShouldThrottleContent(const GURL& content_origin,
                                                   int width,
                                                   int height,
                                                   bool* cross_origin) const {
  DCHECK(cross_origin);
  *cross_origin = true;

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
    *cross_origin = false;
    return false;
  }

  // Whitelisted plugin origins are also essential.
  if (origin_whitelist_.count(content_origin)) {
    RecordDecisionMetric(HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_WHITELISTED);
    return false;
  }

  // Cross-origin plugin content is peripheral if smaller than a maximum size.
  bool content_is_small = width < kPeripheralContentMaxWidth ||
                          height < kPeripheralContentMaxHeight;

  if (content_is_small)
    RecordDecisionMetric(HEURISTIC_DECISION_PERIPHERAL);
  else
    RecordDecisionMetric(HEURISTIC_DECISION_ESSENTIAL_CROSS_ORIGIN_BIG);

  return content_is_small;
}

void PluginPowerSaverHelper::RegisterPeripheralPlugin(
    const GURL& content_origin,
    const base::Closure& unthrottle_callback) {
  peripheral_plugins_.push_back(
      PeripheralPlugin(content_origin, unthrottle_callback));
}

void PluginPowerSaverHelper::WhitelistContentOrigin(
    const GURL& content_origin) {
  if (origin_whitelist_.insert(content_origin).second) {
    Send(new FrameHostMsg_PluginContentOriginAllowed(
        render_frame()->GetRoutingID(), content_origin));
  }
}

}  // namespace content
