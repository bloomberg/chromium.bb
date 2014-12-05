// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEB_GESTURE_CURVE_IMPL_H_
#define CONTENT_CHILD_WEB_GESTURE_CURVE_IMPL_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
class WebGestureCurveTarget;
}

namespace ui {
class GestureCurve;
}

namespace content {

class CONTENT_EXPORT WebGestureCurveImpl
    : public NON_EXPORTED_BASE(blink::WebGestureCurve) {
 public:
  static scoped_ptr<blink::WebGestureCurve> CreateFromDefaultPlatformCurve(
      const gfx::Vector2dF& initial_velocity,
      const gfx::Vector2dF& initial_offset,
      bool on_main_thread);
  static scoped_ptr<blink::WebGestureCurve> CreateFromUICurveForTesting(
      scoped_ptr<ui::GestureCurve> curve,
      const gfx::Vector2dF& initial_offset);

  virtual ~WebGestureCurveImpl();

  // WebGestureCurve implementation.
  virtual bool apply(double time,
                     blink::WebGestureCurveTarget* target) override;

 private:
  enum class ThreadType {
    MAIN,
    IMPL,
    TEST
  };

  WebGestureCurveImpl(scoped_ptr<ui::GestureCurve> curve,
                      const gfx::Vector2dF& initial_offset,
                      ThreadType animating_thread_type);

  scoped_ptr<ui::GestureCurve> curve_;

  gfx::Vector2dF last_offset_;

  ThreadType animating_thread_type_;
  int64 ticks_since_first_animate_;
  double first_animate_time_;
  double last_animate_time_;

  DISALLOW_COPY_AND_ASSIGN(WebGestureCurveImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_WEB_GESTURE_CURVE_IMPL_H_
