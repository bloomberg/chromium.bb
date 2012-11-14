// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/frame_rate_counter.h"

#include <cmath>

#include "base/metrics/histogram.h"
#include "cc/proxy.h"

namespace cc {

const double FrameRateCounter::kFrameTooFast = 1.0 / 70.0; // measured in seconds
const double FrameRateCounter::kFrameTooSlow = 1.0 / 4.0;
const double FrameRateCounter::kDroppedFrameTime = 1.0 / 50.0;

// safeMod works on -1, returning m-1 in that case.
static inline int safeMod(int number, int modulus)
{
    return (number + modulus) % modulus;
}

// static
scoped_ptr<FrameRateCounter> FrameRateCounter::create(bool hasImplThread) {
  return make_scoped_ptr(new FrameRateCounter(hasImplThread));
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

FrameRateCounter::FrameRateCounter(bool hasImplThread)
    : m_hasImplThread(hasImplThread)
    , m_currentFrameNumber(1)
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

    if (m_hasImplThread && m_currentFrameNumber > 0) {
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
    double delta = intervalBetweenConsecutiveFrames.InSecondsF();
    bool schedulerAllowsDoubleFrames = !m_hasImplThread;
    bool intervalTooFast = schedulerAllowsDoubleFrames ? delta < kFrameTooFast : delta <= 0.0;
    bool intervalTooSlow = delta > kFrameTooSlow;
    return intervalTooFast || intervalTooSlow;
}

bool FrameRateCounter::isBadFrame(int frameNumber) const
{
    return isBadFrameInterval(frameInterval(frameNumber));
}

double FrameRateCounter::getAverageFPS() const
{
    int frameNumber = m_currentFrameNumber - 1;
    int frameCount = 0;
    double frameTimesTotal = 0;
    double averageFPS = 0;

    // Walk backwards through the samples looking for a run of good frame
    // timings from which to compute the mean.
    //
    // Slow frames occur just because the user is inactive, and should be
    // ignored. Fast frames are ignored if the scheduler is in single-thread
    // mode in order to represent the true frame rate in spite of the fact that
    // the first few swapbuffers happen instantly which skews the statistics
    // too much for short lived animations.
    //
    // isBadFrameInterval encapsulates the frame too slow/frame too fast logic.

    while (frameIndex(frameNumber) != frameIndex(m_currentFrameNumber) && frameNumber >= 0 && frameTimesTotal < 1.0) {
        base::TimeDelta delta = frameInterval(frameNumber);

        if (!isBadFrameInterval(delta)) {
            frameCount++;
            frameTimesTotal += delta.InSecondsF();
        } else if (frameCount)
            break;

        frameNumber--;
    }

    if (frameCount)
        averageFPS = frameCount / frameTimesTotal;

    return averageFPS;
}

base::TimeTicks FrameRateCounter::timeStampOfRecentFrame(int n) const
{
    DCHECK(n >= 0);
    DCHECK(n < kTimeStampHistorySize);
    int desiredIndex = (frameIndex(m_currentFrameNumber) + n) % kTimeStampHistorySize;
    return m_timeStampHistory[desiredIndex];
}

}  // namespace cc
