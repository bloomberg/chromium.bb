// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/plugin_instance_throttler_impl.h"

#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/color_utils.h"
#include "url/gurl.h"

namespace content {

namespace {

// When we give up waiting for a suitable preview frame, and simply suspend
// the plugin where it's at. In milliseconds.
const int kThrottleTimeout = 5000;

// Threshold for 'boring' score to accept a frame as good enough to be a
// representative keyframe. Units are the ratio of all pixels that are within
// the most common luma bin. The same threshold is used for history thumbnails.
const double kAcceptableFrameMaximumBoringness = 0.94;

const int kMinimumConsecutiveInterestingFrames = 4;

}  // namespace

// static
scoped_ptr<PluginInstanceThrottler> PluginInstanceThrottler::Get(
    RenderFrame* frame,
    const GURL& plugin_url,
    PluginPowerSaverMode power_saver_mode) {
  if (power_saver_mode == PluginPowerSaverMode::POWER_SAVER_MODE_ESSENTIAL)
    return nullptr;

  bool power_saver_enabled =
      power_saver_mode ==
      PluginPowerSaverMode::POWER_SAVER_MODE_PERIPHERAL_THROTTLED;
  return make_scoped_ptr(
      new PluginInstanceThrottlerImpl(frame, plugin_url, power_saver_enabled));
}

// static
void PluginInstanceThrottler::RecordUnthrottleMethodMetric(
    PluginInstanceThrottlerImpl::PowerSaverUnthrottleMethod method) {
  UMA_HISTOGRAM_ENUMERATION(
      "Plugin.PowerSaver.Unthrottle", method,
      PluginInstanceThrottler::UNTHROTTLE_METHOD_NUM_ITEMS);
}

PluginInstanceThrottlerImpl::PluginInstanceThrottlerImpl(
    RenderFrame* frame,
    const GURL& plugin_url,
    bool power_saver_enabled)
    : state_(power_saver_enabled ? POWER_SAVER_ENABLED_AWAITING_KEYFRAME
                                 : POWER_SAVER_DISABLED),
      is_hidden_for_placeholder_(false),
      consecutive_interesting_frames_(0),
      keyframe_extraction_timed_out_(false),
      weak_factory_(this) {
  // To collect UMAs, register peripheral content even if power saver disabled.
  if (frame) {
    frame->RegisterPeripheralPlugin(
        plugin_url.GetOrigin(),
        base::Bind(&PluginInstanceThrottlerImpl::MarkPluginEssential,
                   weak_factory_.GetWeakPtr(), UNTHROTTLE_METHOD_BY_WHITELIST));
  }

  if (power_saver_enabled) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PluginInstanceThrottlerImpl::TimeoutKeyframeExtraction,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kThrottleTimeout));
  }
}

PluginInstanceThrottlerImpl::~PluginInstanceThrottlerImpl() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnThrottlerDestroyed());
  if (state_ != PLUGIN_INSTANCE_MARKED_ESSENTIAL)
    RecordUnthrottleMethodMetric(UNTHROTTLE_METHOD_NEVER);
}

void PluginInstanceThrottlerImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PluginInstanceThrottlerImpl::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool PluginInstanceThrottlerImpl::IsThrottled() const {
  return state_ == POWER_SAVER_ENABLED_PLUGIN_THROTTLED;
}

bool PluginInstanceThrottlerImpl::IsHiddenForPlaceholder() const {
  return is_hidden_for_placeholder_;
}

void PluginInstanceThrottlerImpl::MarkPluginEssential(
    PowerSaverUnthrottleMethod method) {
  if (state_ == PLUGIN_INSTANCE_MARKED_ESSENTIAL)
    return;

  bool was_throttled = IsThrottled();
  state_ = PLUGIN_INSTANCE_MARKED_ESSENTIAL;
  RecordUnthrottleMethodMetric(method);

  if (was_throttled)
    FOR_EACH_OBSERVER(Observer, observer_list_, OnThrottleStateChange());
}

void PluginInstanceThrottlerImpl::SetHiddenForPlaceholder(bool hidden) {
  is_hidden_for_placeholder_ = hidden;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnHiddenForPlaceholder(hidden));
}

void PluginInstanceThrottlerImpl::OnImageFlush(const SkBitmap* bitmap) {
  DCHECK(needs_representative_keyframe());
  if (!bitmap)
    return;

  double boring_score = color_utils::CalculateBoringScore(*bitmap);
  if (boring_score <= kAcceptableFrameMaximumBoringness)
    ++consecutive_interesting_frames_;
  else
    consecutive_interesting_frames_ = 0;

  if (keyframe_extraction_timed_out_ ||
      consecutive_interesting_frames_ >= kMinimumConsecutiveInterestingFrames) {
    FOR_EACH_OBSERVER(Observer, observer_list_, OnKeyframeExtracted(bitmap));
    EngageThrottle();
  }
}

bool PluginInstanceThrottlerImpl::ConsumeInputEvent(
    const blink::WebInputEvent& event) {
  // Always allow right-clicks through so users may verify it's a plug-in.
  // TODO(tommycli): We should instead show a custom context menu (probably
  // using PluginPlaceholder) so users aren't confused and try to click the
  // Flash-internal 'Play' menu item. This is a stopgap solution.
  if (event.modifiers & blink::WebInputEvent::Modifiers::RightButtonDown)
    return false;

  if (state_ != PLUGIN_INSTANCE_MARKED_ESSENTIAL &&
      event.type == blink::WebInputEvent::MouseUp &&
      (event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    bool was_throttled = IsThrottled();
    MarkPluginEssential(UNTHROTTLE_METHOD_BY_CLICK);
    return was_throttled;
  }

  return IsThrottled();
}

void PluginInstanceThrottlerImpl::EngageThrottle() {
  if (state_ != POWER_SAVER_ENABLED_AWAITING_KEYFRAME)
    return;

  state_ = POWER_SAVER_ENABLED_PLUGIN_THROTTLED;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnThrottleStateChange());
}

void PluginInstanceThrottlerImpl::TimeoutKeyframeExtraction() {
  keyframe_extraction_timed_out_ = true;
}

}  // namespace content
