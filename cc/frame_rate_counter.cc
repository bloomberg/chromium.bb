// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCFrameRateCounter.h"

#include <cmath>

#include "base/metrics/histogram.h"
#include "cc/proxy.h"

namespace cc {

const double FrameRateCounter::kFrameTooFast = 1.0 / 70.0; // measured in seconds
const double FrameRateCounter::kFrameTooSlow = 1.0 / 12.0;
const double FrameRateCounter::kDroppedFrameTime = 1.0 / 50.0;

// safeMod works on -1, returning m-1 in that case.
static inline int safeMod(int number, int modulus)
{
    return (number + modulus) % modulus;
}

// static
scoped_ptr<FrameRateCounter> FrameRateCounter::create() {
  return make_scoped_ptr(new FrameRateCounter());
}

inline base::TimeDelta FrameRateCounter::frameInterval(int frameNumber) const
{
    return m_timeStampHistory[frameIndex(frameNumber)] -
        m_timeStampHistory[frameIndex(frameNumber - 1)];
}

inline int FrameRateCounter::frameIndex(int frameNumber) const
{
    return safeMod(frameNumber, kTimeStampHistorySize);
}

FrameRateCounter::FrameRateCounter()
    : m_currentFrameNumber(1)
    , m_droppedFrameCount(0)
{
    m_timeStampHistory[0] = base::TimeTicks::Now();
    m_timeStampHistory[1] = m_timeStampHistory[0];
    for (int i = 2; i < kTimeStampHistorySize; i++)
        m_timeStampHistory[i] = base::TimeTicks();
}

void FrameRateCounter::markBeginningOfFrame(base::TimeTicks timestamp)
{
    m_timeStampHistory[frameIndex(m_currentFrameNumber)] = timestamp;
    base::TimeDelta frameIntervalSeconds = frameInterval(m_currentFrameNumber);

    if (Proxy::hasImplThread() && m_currentFrameNumber > 0) {
        HISTOGRAM_CUSTOM_COUNTS("Renderer4.CompositorThreadImplDrawDelay", frameIntervalSeconds.InMilliseconds(), 1, 120, 60);
    }

    if (!isBadFrameInterval(frameIntervalSeconds) &&
        frameIntervalSeconds.InSecondsF() > kDroppedFrameTime)
        ++m_droppedFrameCount;
}

void FrameRateCounter::markEndOfFrame()
{
    m_currentFrameNumber += 1;
}

bool FrameRateCounter::isBadFrameInterval(base::TimeDelta intervalBetweenConsecutiveFrames) const
{
    bool schedulerAllowsDoubleFrames = !Proxy::hasImplThread();
    bool intervalTooFast = schedulerAllowsDoubleFrames && intervalBetweenConsecutiveFrames.InSecondsF() < kFrameTooFast;
    bool intervalTooSlow = intervalBetweenConsecutiveFrames.InSecondsF() > kFrameTooSlow;
    return intervalTooFast || intervalTooSlow;
}

bool FrameRateCounter::isBadFrame(int frameNumber) const
{
    return isBadFrameInterval(frameInterval(frameNumber));
}

void FrameRateCounter::getAverageFPSAndStandardDeviation(double& averageFPS, double& standardDeviation) const
{
    int frame = m_currentFrameNumber - 1;
    averageFPS = 0;
    int averageFPSCount = 0;
    double fpsVarianceNumerator = 0;

    // Walk backwards through the samples looking for a run of good frame
    // timings from which to compute the mean and standard deviation.
    //
    // Slow frames occur just because the user is inactive, and should be
    // ignored. Fast frames are ignored if the scheduler is in single-thread
    // mode in order to represent the true frame rate in spite of the fact that
    // the first few swapbuffers happen instantly which skews the statistics
    // too much for short lived animations.
    //
    // isBadFrame encapsulates the frame too slow/frame too fast logic.
    while (1) {
        if (!isBadFrame(frame)) {
            averageFPSCount++;
            base::TimeDelta secForLastFrame = m_timeStampHistory[frameIndex(frame)] -
                                              m_timeStampHistory[frameIndex(frame - 1)];
            double x = 1.0 / secForLastFrame.InSecondsF();
            double deltaFromAverage = x - averageFPS;
            // Change with caution - numerics. http://en.wikipedia.org/wiki/Standard_deviation
            averageFPS = averageFPS + deltaFromAverage / averageFPSCount;
            fpsVarianceNumerator = fpsVarianceNumerator + deltaFromAverage * (x - averageFPS);
        }
        if (averageFPSCount && isBadFrame(frame)) {
            // We've gathered a run of good samples, so stop.
            break;
        }
        --frame;
        if (frameIndex(frame) == frameIndex(m_currentFrameNumber) || frame < 0) {
            // We've gone through all available historical data, so stop.
            break;
        }
    }

    standardDeviation = sqrt(fpsVarianceNumerator / averageFPSCount);
}

base::TimeTicks FrameRateCounter::timeStampOfRecentFrame(int n)
{
    DCHECK(n >= 0);
    DCHECK(n < kTimeStampHistorySize);
    int desiredIndex = (frameIndex(m_currentFrameNumber) + n) % kTimeStampHistorySize;
    return m_timeStampHistory[desiredIndex];
}

}  // namespace cc

