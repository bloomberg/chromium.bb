// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_INSTANCE_THROTTLER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_INSTANCE_THROTTLER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "third_party/WebKit/public/platform/WebRect.h"

namespace blink {
class WebInputEvent;
}

class SkBitmap;

namespace content {

class CONTENT_EXPORT PluginInstanceThrottlerImpl
    : public PluginInstanceThrottler {
 public:
  PluginInstanceThrottlerImpl(RenderFrame* frame,
                              const GURL& plugin_url,
                              bool power_saver_enabled);

  ~PluginInstanceThrottlerImpl() override;

  // PluginInstanceThrottler implementation:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool IsThrottled() const override;
  bool IsHiddenForPlaceholder() const override;
  void MarkPluginEssential(PowerSaverUnthrottleMethod method) override;
  void SetHiddenForPlaceholder(bool hidden) override;

  bool needs_representative_keyframe() const {
    return state_ == POWER_SAVER_ENABLED_AWAITING_KEYFRAME;
  }

  bool power_saver_enabled() const {
    return state_ == POWER_SAVER_ENABLED_AWAITING_KEYFRAME ||
           state_ == POWER_SAVER_ENABLED_PLUGIN_THROTTLED;
  }

  // Called when the plugin flushes it's graphics context. Supplies the
  // throttler with a candidate to use as the representative keyframe.
  void OnImageFlush(const SkBitmap* bitmap);

  // Returns true if |event| was handled and shouldn't be further processed.
  bool ConsumeInputEvent(const blink::WebInputEvent& event);

 private:
  friend class PluginInstanceThrottlerImplTest;

  enum State {
    // Initial state if Power Saver is disabled. We are just collecting metrics.
    POWER_SAVER_DISABLED,
    // Initial state if Power Saver is enabled. Waiting for a keyframe.
    POWER_SAVER_ENABLED_AWAITING_KEYFRAME,
    // We've chosen a keyframe and the plug-in is throttled.
    POWER_SAVER_ENABLED_PLUGIN_THROTTLED,
    // Plugin instance is no longer considered peripheral. This can happen from
    // a user click, whitelisting, or some other reason. We can end up in this
    // state regardless of whether power saver is enabled.
    PLUGIN_INSTANCE_MARKED_ESSENTIAL
  };

  void EngageThrottle();

  void TimeoutKeyframeExtraction();

  State state_;

  bool is_hidden_for_placeholder_;

  // Number of consecutive interesting frames we've encountered.
  int consecutive_interesting_frames_;

  // If true, take the next frame as the keyframe regardless of interestingness.
  bool keyframe_extraction_timed_out_;

  ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<PluginInstanceThrottlerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstanceThrottlerImpl);
};
}

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_INSTANCE_THROTTLER_IMPL_H_
