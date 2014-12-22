// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_plugin_instance_throttler.h"

#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/time/time.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/color_utils.h"
#include "url/gurl.h"

namespace content {

namespace {

static const int kInfiniteRatio = 99999;

#define UMA_HISTOGRAM_ASPECT_RATIO(name, width, height) \
  UMA_HISTOGRAM_SPARSE_SLOWLY(                          \
      name, (height) ? ((width)*100) / (height) : kInfiniteRatio);

// Histogram tracking prevalence of tiny Flash instances. Units in pixels.
enum PluginFlashTinyContentSize {
  TINY_CONTENT_SIZE_1_1 = 0,
  TINY_CONTENT_SIZE_5_5 = 1,
  TINY_CONTENT_SIZE_10_10 = 2,
  TINY_CONTENT_SIZE_LARGE = 3,
  TINY_CONTENT_SIZE_NUM_ITEMS
};

const char kFlashClickSizeAspectRatioHistogram[] =
    "Plugin.Flash.ClickSize.AspectRatio";
const char kFlashClickSizeHeightHistogram[] = "Plugin.Flash.ClickSize.Height";
const char kFlashClickSizeWidthHistogram[] = "Plugin.Flash.ClickSize.Width";
const char kFlashTinyContentSizeHistogram[] = "Plugin.Flash.TinyContentSize";
const char kPowerSaverUnthrottleHistogram[] = "Plugin.PowerSaver.Unthrottle";

// Record size metrics for all Flash instances.
void RecordFlashSizeMetric(int width, int height) {
  PluginFlashTinyContentSize size = TINY_CONTENT_SIZE_LARGE;

  if (width <= 1 && height <= 1)
    size = TINY_CONTENT_SIZE_1_1;
  else if (width <= 5 && height <= 5)
    size = TINY_CONTENT_SIZE_5_5;
  else if (width <= 10 && height <= 10)
    size = TINY_CONTENT_SIZE_10_10;

  UMA_HISTOGRAM_ENUMERATION(kFlashTinyContentSizeHistogram, size,
                            TINY_CONTENT_SIZE_NUM_ITEMS);
}

void RecordUnthrottleMethodMetric(
    PepperPluginInstanceThrottler::PowerSaverUnthrottleMethod method) {
  UMA_HISTOGRAM_ENUMERATION(
      kPowerSaverUnthrottleHistogram, method,
      PepperPluginInstanceThrottler::UNTHROTTLE_METHOD_NUM_ITEMS);
}

// Records size metrics for Flash instances that are clicked.
void RecordFlashClickSizeMetric(int width, int height) {
  base::HistogramBase* width_histogram = base::LinearHistogram::FactoryGet(
      kFlashClickSizeWidthHistogram,
      0,    // minimum width
      500,  // maximum width
      100,  // number of buckets.
      base::HistogramBase::kUmaTargetedHistogramFlag);
  width_histogram->Add(width);

  base::HistogramBase* height_histogram = base::LinearHistogram::FactoryGet(
      kFlashClickSizeHeightHistogram,
      0,    // minimum height
      400,  // maximum height
      100,  // number of buckets.
      base::HistogramBase::kUmaTargetedHistogramFlag);
  height_histogram->Add(height);

  UMA_HISTOGRAM_ASPECT_RATIO(kFlashClickSizeAspectRatioHistogram, width,
                             height);
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

PepperPluginInstanceThrottler::PepperPluginInstanceThrottler(
    RenderFrame* frame,
    const blink::WebRect& bounds,
    const std::string& module_name,
    const GURL& plugin_url,
    RenderFrame::PluginPowerSaverMode power_saver_mode,
    const base::Closure& throttle_change_callback)
    : bounds_(bounds),
      throttle_change_callback_(throttle_change_callback),
      is_flash_plugin_(module_name == kFlashPluginName),
      needs_representative_keyframe_(false),
      consecutive_interesting_frames_(0),
      has_been_clicked_(false),
      power_saver_enabled_(false),
      is_peripheral_content_(power_saver_mode !=
                             RenderFrame::POWER_SAVER_MODE_ESSENTIAL),
      plugin_throttled_(false),
      weak_factory_(this) {
  if (is_flash_plugin_ && RenderThread::Get()) {
    RenderThread::Get()->RecordAction(
        base::UserMetricsAction("Flash.PluginInstanceCreated"));
    RecordFlashSizeMetric(bounds.width, bounds.height);
  }

  power_saver_enabled_ =
      is_flash_plugin_ &&
      power_saver_mode == RenderFrame::POWER_SAVER_MODE_PERIPHERAL_THROTTLED;

  GURL content_origin = plugin_url.GetOrigin();

  // To collect UMAs, register peripheral content even if power saver disabled.
  if (frame) {
    frame->RegisterPeripheralPlugin(
        content_origin,
        base::Bind(&PepperPluginInstanceThrottler::DisablePowerSaver,
                   weak_factory_.GetWeakPtr(), UNTHROTTLE_METHOD_BY_WHITELIST));
  }

  if (power_saver_enabled_) {
    needs_representative_keyframe_ = true;
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PepperPluginInstanceThrottler::SetPluginThrottled,
                   weak_factory_.GetWeakPtr(), true /* throttled */),
        base::TimeDelta::FromMilliseconds(kThrottleTimeout));
  }
}

PepperPluginInstanceThrottler::~PepperPluginInstanceThrottler() {
}

void PepperPluginInstanceThrottler::OnImageFlush(const SkBitmap* bitmap) {
  if (!needs_representative_keyframe_ || !bitmap)
    return;

  double boring_score = color_utils::CalculateBoringScore(*bitmap);
  if (boring_score <= kAcceptableFrameMaximumBoringness)
    ++consecutive_interesting_frames_;
  else
    consecutive_interesting_frames_ = 0;

  if (consecutive_interesting_frames_ >= kMinimumConsecutiveInterestingFrames)
    SetPluginThrottled(true);
}

bool PepperPluginInstanceThrottler::ConsumeInputEvent(
    const blink::WebInputEvent& event) {
  // Always allow right-clicks through so users may verify it's a plug-in.
  // TODO(tommycli): We should instead show a custom context menu (probably
  // using PluginPlaceholder) so users aren't confused and try to click the
  // Flash-internal 'Play' menu item. This is a stopgap solution.
  if (event.modifiers & blink::WebInputEvent::Modifiers::RightButtonDown)
    return false;

  if (!has_been_clicked_ && is_flash_plugin_ &&
      event.type == blink::WebInputEvent::MouseDown &&
      (event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    has_been_clicked_ = true;
    RecordFlashClickSizeMetric(bounds_.width, bounds_.height);
  }

  if (is_peripheral_content_ && event.type == blink::WebInputEvent::MouseUp &&
      (event.modifiers & blink::WebInputEvent::LeftButtonDown)) {
    is_peripheral_content_ = false;
    power_saver_enabled_ = false;
    needs_representative_keyframe_ = false;

    RecordUnthrottleMethodMetric(UNTHROTTLE_METHOD_BY_CLICK);

    if (plugin_throttled_) {
      SetPluginThrottled(false /* throttled */);
      return true;
    }
  }

  return plugin_throttled_;
}

void PepperPluginInstanceThrottler::DisablePowerSaver(
    PowerSaverUnthrottleMethod method) {
  if (!is_peripheral_content_)
    return;

  is_peripheral_content_ = false;
  power_saver_enabled_ = false;
  SetPluginThrottled(false);

  RecordUnthrottleMethodMetric(method);
}

void PepperPluginInstanceThrottler::SetPluginThrottled(bool throttled) {
  // Do not throttle if we've already disabled power saver.
  if (!power_saver_enabled_ && throttled)
    return;

  // Once we change the throttle state, we will never need the snapshot again.
  needs_representative_keyframe_ = false;

  plugin_throttled_ = throttled;
  throttle_change_callback_.Run();
}

}  // namespace content
