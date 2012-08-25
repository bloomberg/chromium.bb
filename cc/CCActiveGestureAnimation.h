// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCActiveGestureAnimation_h
#define CCActiveGestureAnimation_h

#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCGestureCurve;
class CCGestureCurveTarget;

class CCActiveGestureAnimation {
    WTF_MAKE_NONCOPYABLE(CCActiveGestureAnimation);
public:
    static PassOwnPtr<CCActiveGestureAnimation> create(PassOwnPtr<CCGestureCurve>, CCGestureCurveTarget*);
    ~CCActiveGestureAnimation();

    bool animate(double monotonicTime);

private:
    CCActiveGestureAnimation(PassOwnPtr<CCGestureCurve>, CCGestureCurveTarget*);

    double m_startTime;
    double m_waitingForFirstTick;
    OwnPtr<CCGestureCurve> m_gestureCurve;
    CCGestureCurveTarget* m_gestureCurveTarget;
};

} // namespace WebCore

#endif
