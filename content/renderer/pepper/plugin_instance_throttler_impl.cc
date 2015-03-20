// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/plugin_instance_throttler_impl.h"

#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "ui/gfx/color_utils.h"
#include "url/gurl.h"

namespace content {

namespace {

// Threshold for 'boring' score to accept a frame as good enough to be a
// representative keyframe. Units are the ratio of all pixels that are within
// the most common luma bin. The same threshold is used for history thumbnails.
const double kAcceptableFrameMaximumBoringness = 0.94;

const int kMinimumConsecutiveInterestingFrames = 4;

// When plugin audio is throttled, the plugin will sometimes stop generating
// video frames. We use this timeout to prevent waiting forever for a good
// poster image. Chosen arbitrarily.
const int kAudioThrottledFrameTimeoutMilliseconds = 500;

}  // namespace

// static
const int PluginInstanceThrottlerImpl::kMaximumFramesToExamine = 150;

// static
scoped_ptr<PluginInstanceThrottler> PluginInstanceThrottler::Create(
    bool power_saver_enabled) {
  return make_scoped_ptr(new PluginInstanceThrottlerImpl(power_saver_enabled));
}

// static
void PluginInstanceThrottler::RecordUnthrottleMethodMetric(
    PluginInstanceThrottlerImpl::PowerSaverUnthrottleMethod method) {
  UMA_HISTOGRAM_ENUMERATION(
      "Plugin.PowerSaver.Unthrottle", method,
      PluginInstanceThrottler::UNTHROTTLE_METHOD_NUM_ITEMS);
}

PluginInstanceThrottlerImpl::PluginInstanceThrottlerImpl(
    bool power_saver_enabled)
    : state_(power_saver_enabled ? THROTTLER_STATE_AWAITING_KEYFRAME
                                 : THROTTLER_STATE_POWER_SAVER_DISABLED),
      is_hidden_for_placeholder_(false),
      web_plugin_(nullptr),
      consecutive_interesting_frames_(0),
      frames_examined_(0),
      audio_throttled_(false),
      audio_throttled_frame_timeout_(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(
              kAudioThrottledFrameTimeoutMilliseconds),
          this,
          &PluginInstanceThrottlerImpl::EngageThrottle),
      weak_factory_(this) {
}

PluginInstanceThrottlerImpl::~PluginInstanceThrottlerImpl() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnThrottlerDestroyed());
  if (state_ != THROTTLER_STATE_MARKED_ESSENTIAL)
    RecordUnthrottleMethodMetric(UNTHROTTLE_METHOD_NEVER);
}

void PluginInstanceThrottlerImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PluginInstanceThrottlerImpl::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool PluginInstanceThrottlerImpl::IsThrottled() const {
  return state_ == THROTTLER_STATE_PLUGIN_THROTTLED;
}

bool PluginInstanceThrottlerImpl::IsHiddenForPlaceholder() const {
  return is_hidden_for_placeholder_;
}

void PluginInstanceThrottlerImpl::MarkPluginEssential(
    PowerSaverUnthrottleMethod method) {
  if (state_ == THROTTLER_STATE_MARKED_ESSENTIAL)
    return;

  bool was_throttled = IsThrottled();
  state_ = THROTTLER_STATE_MARKED_ESSENTIAL;
  RecordUnthrottleMethodMetric(method);

  if (was_throttled)
    FOR_EACH_OBSERVER(Observer, observer_list_, OnThrottleStateChange());
}

void PluginInstanceThrottlerImpl::SetHiddenForPlaceholder(bool hidden) {
  is_hidden_for_placeholder_ = hidden;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnHiddenForPlaceholder(hidden));
}

blink::WebPlugin* PluginInstanceThrottlerImpl::GetWebPlugin() const {
  DCHECK(web_plugin_);
  return web_plugin_;
}

void PluginInstanceThrottlerImpl::NotifyAudioThrottled() {
  audio_throttled_ = true;
  audio_throttled_frame_timeout_.Reset();
}

void PluginInstanceThrottlerImpl::SetWebPlugin(blink::WebPlugin* web_plugin) {
  DCHECK(!web_plugin_);
  web_plugin_ = web_plugin;
}

void PluginInstanceThrottlerImpl::Initialize(
    RenderFrameImpl* frame,
    const GURL& content_origin,
    const std::string& plugin_module_name,
    const blink::WebRect& bounds) {
  // |frame| may be nullptr in tests.
  if (frame) {
    PluginPowerSaverHelper* helper = frame->plugin_power_saver_helper();
    bool cross_origin_main_content = false;
    if (!helper->ShouldThrottleContent(content_origin, plugin_module_name,
                                       bounds.width, bounds.height,
                                       &cross_origin_main_content)) {
      state_ = THROTTLER_STATE_MARKED_ESSENTIAL;

      if (cross_origin_main_content)
        helper->WhitelistContentOrigin(content_origin);

      return;
    }

    // To collect UMAs, register peripheral content even if power saver mode
    // is disabled.
    helper->RegisterPeripheralPlugin(
        content_origin,
        base::Bind(&PluginInstanceThrottlerImpl::MarkPluginEssential,
                   weak_factory_.GetWeakPtr(), UNTHROTTLE_METHOD_BY_WHITELIST));
  }
}

void PluginInstanceThrottlerImpl::OnImageFlush(const SkBitmap* bitmap) {
  DCHECK(needs_representative_keyframe());
  if (!bitmap)
    return;

  ++frames_examined_;

  double boring_score = color_utils::CalculateBoringScore(*bitmap);
  if (boring_score <= kAcceptableFrameMaximumBoringness)
    ++consecutive_interesting_frames_;
  else
    consecutive_interesting_frames_ = 0;

  // Does not make a copy, just takes a reference to the underlying pixel data.
  last_received_frame_ = *bitmap;

  if (audio_throttled_)
    audio_throttled_frame_timeout_.Reset();

  if (frames_examined_ >= kMaximumFramesToExamine ||
      consecutive_interesting_frames_ >= kMinimumConsecutiveInterestingFrames) {
    EngageThrottle();
  }
}

bool PluginInstanceThrottlerImpl::ConsumeInputEvent(
    const blink::WebInputEvent& event) {
  // Always allow right-clicks through so users may verify it's a plugin.
  // TODO(tommycli): We should instead show a custom context menu (probably
  // using PluginPlaceholder) so users aren't confused and try to click the
  // Flash-internal 'Play' menu item. This is a stopgap solution.
  if (event.modifiers & blink::WebInputEvent::Modifiers::RightButtonDown)
    return false;

  if (state_ != THROTTLER_STATE_MARKED_ESSENTIAL &&
      event.type == blink::WebInputEvent::MouseUp &&
      (event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    bool was_throttled = IsThrottled();
    MarkPluginEssential(UNTHROTTLE_METHOD_BY_CLICK);
    return was_throttled;
  }

  return IsThrottled();
}

void PluginInstanceThrottlerImpl::EngageThrottle() {
  if (state_ != THROTTLER_STATE_AWAITING_KEYFRAME)
    return;

  if (!last_received_frame_.empty()) {
    FOR_EACH_OBSERVER(Observer, observer_list_,
                      OnKeyframeExtracted(&last_received_frame_));

    // Release our reference to the underlying pixel data.
    last_received_frame_.reset();
  }

  state_ = THROTTLER_STATE_PLUGIN_THROTTLED;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnThrottleStateChange());
}

}  // namespace content
