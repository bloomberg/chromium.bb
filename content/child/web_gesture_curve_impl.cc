// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_gesture_curve_impl.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureCurveTarget.h"
#include "ui/events/gestures/fling_curve.h"
#include "ui/gfx/vector2d.h"

#if defined(OS_ANDROID)
#include "ui/events/android/scroller.h"
#endif

using blink::WebGestureCurve;

namespace content {
namespace {

scoped_ptr<ui::GestureCurve> CreateDefaultPlatformCurve(
    const gfx::Vector2dF& initial_velocity) {
  DCHECK(!initial_velocity.IsZero());
#if defined(OS_ANDROID)
  auto scroller = make_scoped_ptr(new ui::Scroller(ui::Scroller::Config()));
  scroller->Fling(0,
                  0,
                  initial_velocity.x(),
                  initial_velocity.y(),
                  INT_MIN,
                  INT_MAX,
                  INT_MIN,
                  INT_MAX,
                  base::TimeTicks());
  return scroller.Pass();
#else
  return make_scoped_ptr(
      new ui::FlingCurve(initial_velocity, base::TimeTicks()));
#endif
}

}  // namespace

// static
scoped_ptr<WebGestureCurve> WebGestureCurveImpl::CreateFromDefaultPlatformCurve(
    const gfx::Vector2dF& initial_velocity,
    const gfx::Vector2dF& initial_offset) {
  return CreateFrom(CreateDefaultPlatformCurve(initial_velocity),
                    initial_offset);
}

// static
scoped_ptr<WebGestureCurve> WebGestureCurveImpl::CreateFrom(
    scoped_ptr<ui::GestureCurve> curve,
    const gfx::Vector2dF& initial_offset) {
  return scoped_ptr<WebGestureCurve>(
      new WebGestureCurveImpl(curve.Pass(), initial_offset));
}

WebGestureCurveImpl::WebGestureCurveImpl(scoped_ptr<ui::GestureCurve> curve,
                                         const gfx::Vector2dF& initial_offset)
    : curve_(curve.Pass()), last_offset_(initial_offset) {
}

WebGestureCurveImpl::~WebGestureCurveImpl() {
}

bool WebGestureCurveImpl::apply(double time,
                                blink::WebGestureCurveTarget* target) {
  // If the fling has yet to start, simply return and report true to prevent
  // fling termination.
  if (time <= 0)
    return true;

  const base::TimeTicks time_ticks =
      base::TimeTicks() + base::TimeDelta::FromSecondsD(time);
  gfx::Vector2dF offset, velocity;
  bool still_active =
      curve_->ComputeScrollOffset(time_ticks, &offset, &velocity);

  gfx::Vector2dF delta = offset - last_offset_;
  last_offset_ = offset;

  // As successive timestamps can be arbitrarily close (but monotonic!), don't
  // assume that a zero delta means the curve has terminated.
  if (delta.IsZero())
    return still_active;

  // scrollBy() could delete this curve if the animation is over, so don't touch
  // any member variables after making that call.
  bool did_scroll = target->scrollBy(blink::WebFloatSize(delta),
                                     blink::WebFloatSize(velocity));
  return did_scroll && still_active;
}

}  // namespace content
