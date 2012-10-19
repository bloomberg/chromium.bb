// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCFrameRateCounter_h
#define CCFrameRateCounter_h

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"

namespace cc {

// This class maintains a history of timestamps, and provides functionality to
// intelligently compute average frames per second (and standard deviation).
class CCFrameRateCounter {
public:
    static scoped_ptr<CCFrameRateCounter> create();

    void markBeginningOfFrame(base::TimeTicks timestamp);
    void markEndOfFrame();
    int currentFrameNumber() const { return m_currentFrameNumber; }
    void getAverageFPSAndStandardDeviation(double& averageFPS, double& standardDeviation) const;
    int timeStampHistorySize() const { return kTimeStampHistorySize; }

    // n = 0 returns the oldest frame retained in the history,
    // while n = timeStampHistorySize() - 1 returns the timestamp most recent frame.
    base::TimeTicks timeStampOfRecentFrame(int n);

    // This is a heuristic that can be used to ignore frames in a reasonable way. Returns
    // true if the given frame interval is too fast or too slow, based on constant thresholds.
    bool isBadFrameInterval(base::TimeDelta intervalBetweenConsecutiveFrames) const;

    int droppedFrameCount() const { return m_droppedFrameCount; }

private:
    CCFrameRateCounter();

    base::TimeDelta frameInterval(int frameNumber) const;
    int frameIndex(int frameNumber) const;
    bool isBadFrame(int frameNumber) const;

    // Two thresholds (measured in seconds) that describe what is considered to be a "no-op frame" that should not be counted.
    // - if the frame is too fast, then given our compositor implementation, the frame probably was a no-op and did not draw.
    // - if the frame is too slow, then there is probably not animating content, so we should not pollute the average.
    static const double kFrameTooFast;
    static const double kFrameTooSlow;

    // If a frame takes longer than this threshold (measured in seconds) then we
    // (naively) assume that it missed a screen refresh; that is, we dropped a frame.
    // FIXME: Determine this threshold based on monitor refresh rate, crbug.com/138642.
    static const double kDroppedFrameTime;

    static const int kTimeStampHistorySize = 120;

    int m_currentFrameNumber;
    base::TimeTicks m_timeStampHistory[kTimeStampHistorySize];

    int m_droppedFrameCount;

    DISALLOW_COPY_AND_ASSIGN(CCFrameRateCounter);
};

}  // namespace cc

#endif
