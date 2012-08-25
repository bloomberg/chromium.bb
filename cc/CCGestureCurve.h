// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCGestureCurve_h
#define CCGestureCurve_h

namespace WebCore {

class IntPoint;

class CCGestureCurveTarget {
public:
    virtual ~CCGestureCurveTarget() { }

    virtual void scrollBy(const IntPoint&) = 0;
    // FIXME: add interfaces for scroll(), etc.
};

class CCGestureCurve {
public:
    virtual ~CCGestureCurve() { }

    virtual const char* debugName() const = 0;

    virtual bool apply(double monotonicTime, CCGestureCurveTarget*) = 0;
};

} // namespace WebCore

#endif
