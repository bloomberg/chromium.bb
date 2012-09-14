// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCFrameRateCounter_h
#define CCFrameRateCounter_h

#if USE(ACCELERATED_COMPOSITING)

#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace cc {

// This class maintains a history of timestamps, and provides functionality to
// intelligently compute average frames per second (and standard deviation).
class CCFrameRateCounter {
    WTF_MAKE_NONCOPYABLE(CCFrameRateCounter);
public:
    static PassOwnPtr<CCFrameRateCounter> create()
    {
        return adoptPtr(new CCFrameRateCounter());
    }

    void markBeginningOfFrame(double timestamp);
    void markEndOfFrame();
    int currentFrameNumber() const { return m_currentFrameNumber; }
    void getAverageFPSAndStandardDeviation(double& averageFPS, double& standardDeviation) const;
    int timeStampHistorySize() const { return kTimeStampHistorySize; }

    // n = 0 returns the oldest frame retained in the history,
    // while n = timeStampHistorySize() - 1 returns the timestamp most recent frame.
    double timeStampOfRecentFrame(int /* n */);

    // This is a heuristic that can be used to ignore frames in a reasonable way. Returns
    // true if the given frame interval is too fast or too slow, based on constant thresholds.
    bool isBadFrameInterval(double intervalBetweenConsecutiveFrames) const;

    int droppedFrameCount() const { return m_droppedFrameCount; }

private:
    CCFrameRateCounter();

    double frameInterval(int frameNumber) const;
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
    double m_timeStampHistory[kTimeStampHistorySize];

    int m_droppedFrameCount;
};

} // namespace cc

#endif // USE(ACCELERATED_COMPOSITING)

#endif
