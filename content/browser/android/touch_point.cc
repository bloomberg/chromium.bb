// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/touch_point.h"

#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/time/time.h"

#include "jni/TouchPoint_jni.h"

using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace {

void MaybeAddTouchPoint(JNIEnv* env,
                        jobject pt,
                        float dpi_scale,
                        blink::WebTouchEvent& event) {
  WebTouchPoint::State state = static_cast<WebTouchPoint::State>(
      Java_TouchPoint_getState(env, pt));
  if (state == WebTouchPoint::StateUndefined)
    return;

  // When generating a cancel event from an event of a different type, the
  // touch points are out of sync, so we ensure the points are marked as
  // canceled as well.
  if (event.type == WebTouchEvent::TouchCancel)
    state = WebTouchPoint::StateCancelled;

  // Record the current number of points in the WebTouchEvent
  const int idx = event.touchesLength;
  DCHECK_LT(idx, blink::WebTouchEvent::touchesLengthCap);

  WebTouchPoint wtp;
  wtp.id = Java_TouchPoint_getId(env, pt);
  wtp.state = state;
  wtp.position.x = Java_TouchPoint_getX(env, pt) / dpi_scale;
  wtp.position.y = Java_TouchPoint_getY(env, pt) / dpi_scale;
  // TODO(joth): Raw event co-ordinates.
  wtp.screenPosition = wtp.position;
  wtp.force = Java_TouchPoint_getPressure(env, pt);

  const int radiusMajor = static_cast<int>(
      Java_TouchPoint_getTouchMajor(env, pt) * 0.5f / dpi_scale);
  const int radiusMinor = static_cast<int>(
      Java_TouchPoint_getTouchMinor(env, pt) * 0.5f / dpi_scale);
  const float majorAngleInRadiansClockwiseFromVertical =
      Java_TouchPoint_getOrientation(env, pt);
  const float majorAngleInDegreesClockwiseFromVertical =
      std::isnan(majorAngleInRadiansClockwiseFromVertical)
          ? 0.f : (majorAngleInRadiansClockwiseFromVertical * 180.f) / M_PI;
  // Android provides a major axis orientation clockwise with respect to the
  // vertical of [-90, 90], while the W3C specifies a range of [0, 90].
  if (majorAngleInDegreesClockwiseFromVertical >= 0) {
    wtp.radiusX = radiusMinor;
    wtp.radiusY = radiusMajor;
    wtp.rotationAngle = majorAngleInDegreesClockwiseFromVertical;
  } else {
    wtp.radiusX = radiusMajor;
    wtp.radiusY = radiusMinor;
    wtp.rotationAngle = majorAngleInDegreesClockwiseFromVertical + 90.f;
  }
  DCHECK_GE(wtp.rotationAngle, 0.f);
  DCHECK_LE(wtp.rotationAngle, 90.f);

  // Add the newly created WebTouchPoint to the event
  event.touches[idx] = wtp;
  ++(event.touchesLength);
}

}  // namespace

namespace content {

void TouchPoint::BuildWebTouchEvent(JNIEnv* env,
                                    jint type,
                                    jlong time_ms,
                                    float dpi_scale,
                                    jobjectArray pts,
                                    blink::WebTouchEvent& event) {
  event.type = static_cast<WebTouchEvent::Type>(type);
  event.timeStampSeconds =
      static_cast<double>(time_ms) / base::Time::kMillisecondsPerSecond;
  int arrayLength = env->GetArrayLength(pts);
  // Loop until either all of the input points have been consumed or the output
  // array has been filled
  for (int i = 0; i < arrayLength; i++) {
    jobject pt = env->GetObjectArrayElement(pts, i);
    MaybeAddTouchPoint(env, pt, dpi_scale, event);
    if (event.touchesLength >= event.touchesLengthCap)
      break;
  }
  DCHECK_GT(event.touchesLength, 0U);
}

static void RegisterConstants(JNIEnv* env) {
   Java_TouchPoint_initializeConstants(
       env,
       blink::WebTouchEvent::TouchStart,
       blink::WebTouchEvent::TouchMove,
       blink::WebTouchEvent::TouchEnd,
       blink::WebTouchEvent::TouchCancel,
       blink::WebTouchPoint::StateUndefined,
       blink::WebTouchPoint::StateReleased,
       blink::WebTouchPoint::StatePressed,
       blink::WebTouchPoint::StateMoved,
       blink::WebTouchPoint::StateStationary,
       blink::WebTouchPoint::StateCancelled);
}

bool RegisterTouchPoint(JNIEnv* env) {
  if (!RegisterNativesImpl(env))
    return false;

  RegisterConstants(env);

  return true;
}

}  // namespace content
