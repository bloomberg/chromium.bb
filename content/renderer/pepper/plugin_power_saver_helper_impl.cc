// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/plugin_power_saver_helper_impl.h"

#include "base/metrics/histogram.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/frame_messages.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebView.h"

namespace content {

namespace {

const char kPosterParamName[] = "poster";

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

// Maximum dimensions plug-in content may have while still being considered
// peripheral content. These match the sizes used by Safari.
const int kPeripheralContentMaxWidth = 400;
const int kPeripheralContentMaxHeight = 300;

// Plug-in content below this size in height and width is considered "tiny".
// Tiny content is never peripheral, as tiny plug-ins often serve a critical
// purpose, and the user often cannot find and click to unthrottle it.
const int kPeripheralContentTinySize = 5;

void RecordDecisionMetric(PeripheralHeuristicDecision decision) {
  UMA_HISTOGRAM_ENUMERATION(kPeripheralHeuristicHistogram, decision,
                            HEURISTIC_DECISION_NUM_ITEMS);
}

const char kWebPluginParamHeight[] = "height";
const char kWebPluginParamWidth[] = "width";

// Returns true if valid non-negative height and width extracted.
// When this returns false, |width| and |height| are set to undefined values.
bool ExtractDimensions(const blink::WebPluginParams& params,
                       int* width,
                       int* height) {
  DCHECK_EQ(params.attributeNames.size(), params.attributeValues.size());
  DCHECK(width);
  DCHECK(height);
  bool width_extracted = false;
  bool height_extracted = false;
  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    if (params.attributeNames[i].utf8() == kWebPluginParamWidth) {
      width_extracted =
          base::StringToInt(params.attributeValues[i].utf8(), width);
    } else if (params.attributeNames[i].utf8() == kWebPluginParamHeight) {
      height_extracted =
          base::StringToInt(params.attributeValues[i].utf8(), height);
    }
  }
  return width_extracted && height_extracted && *width >= 0 && *height >= 0;
}

}  // namespace

PluginPowerSaverHelperImpl::PeripheralPlugin::PeripheralPlugin(
    const GURL& content_origin,
    const base::Closure& unthrottle_callback)
    : content_origin(content_origin), unthrottle_callback(unthrottle_callback) {
}

PluginPowerSaverHelperImpl::PeripheralPlugin::~PeripheralPlugin() {
}

PluginPowerSaverHelperImpl::PluginPowerSaverHelperImpl(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

PluginPowerSaverHelperImpl::~PluginPowerSaverHelperImpl() {
}

void PluginPowerSaverHelperImpl::DidCommitProvisionalLoad(
    bool is_new_navigation) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  if (frame->parent())
    return;  // Not a top-level navigation.

  DocumentState* document_state =
      DocumentState::FromDataSource(frame->dataSource());
  NavigationState* navigation_state = document_state->navigation_state();
  if (!navigation_state->was_within_same_page())
    origin_whitelist_.clear();
}

bool PluginPowerSaverHelperImpl::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginPowerSaverHelperImpl, message)
  IPC_MESSAGE_HANDLER(FrameMsg_UpdatePluginContentOriginWhitelist,
                      OnUpdatePluginContentOriginWhitelist)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PluginPowerSaverHelperImpl::OnUpdatePluginContentOriginWhitelist(
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

GURL PluginPowerSaverHelperImpl::GetPluginInstancePosterImage(
    const blink::WebPluginParams& params,
    const GURL& base_url) const {
  DCHECK_EQ(params.attributeNames.size(), params.attributeValues.size());

  GURL content_origin = GURL(params.url).GetOrigin();
  int width = 0;
  int height = 0;

  if (!ExtractDimensions(params, &width, &height))
    return GURL();

  if (!ShouldThrottleContent(content_origin, width, height, nullptr))
    return GURL();

  for (size_t i = 0; i < params.attributeNames.size(); ++i) {
    if (params.attributeNames[i] == kPosterParamName) {
      std::string poster_value(params.attributeValues[i].utf8());
      if (!poster_value.empty())
        return base_url.Resolve(poster_value);
    }
  }
  return GURL();
}

void PluginPowerSaverHelperImpl::RegisterPeripheralPlugin(
    const GURL& content_origin,
    const base::Closure& unthrottle_callback) {
  peripheral_plugins_.push_back(
      PeripheralPlugin(content_origin, unthrottle_callback));
}

bool PluginPowerSaverHelperImpl::ShouldThrottleContent(
    const GURL& content_origin,
    int width,
    int height,
    bool* is_main_attraction) const {
  DCHECK_EQ(content_origin.GetOrigin(), content_origin);
  if (is_main_attraction)
    *is_main_attraction = false;

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
    if (is_main_attraction)
      *is_main_attraction = true;
    return false;
  }

  RecordDecisionMetric(HEURISTIC_DECISION_PERIPHERAL);
  return true;
}

void PluginPowerSaverHelperImpl::WhitelistContentOrigin(
    const GURL& content_origin) {
  DCHECK_EQ(content_origin.GetOrigin(), content_origin);
  if (origin_whitelist_.insert(content_origin).second) {
    Send(new FrameHostMsg_PluginContentOriginAllowed(
        render_frame()->GetRoutingID(), content_origin));
  }
}

}  // namespace content
