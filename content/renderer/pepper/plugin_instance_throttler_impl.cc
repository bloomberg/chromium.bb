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

const char kPowerSaverUnthrottleHistogram[] = "Plugin.PowerSaver.Unthrottle";

void RecordUnthrottleMethodMetric(
    PluginInstanceThrottlerImpl::PowerSaverUnthrottleMethod method) {
  UMA_HISTOGRAM_ENUMERATION(
      kPowerSaverUnthrottleHistogram, method,
      PluginInstanceThrottler::UNTHROTTLE_METHOD_NUM_ITEMS);
}

// When we give up waiting for a suitable preview frame, and simply suspend
// the plugin where it's at. In milliseconds.
const int kThrottleTimeout = 5000;

// Threshold for 'boring' score to accept a frame as good enough to be a
// representative keyframe. Units are the ratio of all pixels that are within
// the most common luma bin. The same threshold is used for history thumbnails.
const double kAcceptableFrameMaximumBoringness = 0.94;

const int kMinimumConsecutiveInterestingFrames = 4;

}  // namespace

PluginInstanceThrottlerImpl::PluginInstanceThrottlerImpl(
    RenderFrame* frame,
    const GURL& plugin_url,
    bool power_saver_enabled)
    : state_(power_saver_enabled ? POWER_SAVER_ENABLED_AWAITING_KEYFRAME
                                 : POWER_SAVER_DISABLED),
      consecutive_interesting_frames_(0),
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
        FROM_HERE, base::Bind(&PluginInstanceThrottlerImpl::EngageThrottle,
                              weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kThrottleTimeout));
  }
}

PluginInstanceThrottlerImpl::~PluginInstanceThrottlerImpl() {
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

void PluginInstanceThrottlerImpl::OnImageFlush(const SkBitmap* bitmap) {
  DCHECK(needs_representative_keyframe());
  if (!bitmap)
    return;

  double boring_score = color_utils::CalculateBoringScore(*bitmap);
  if (boring_score <= kAcceptableFrameMaximumBoringness)
    ++consecutive_interesting_frames_;
  else
    consecutive_interesting_frames_ = 0;

  if (consecutive_interesting_frames_ >= kMinimumConsecutiveInterestingFrames)
    EngageThrottle();
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

}  // namespace content
