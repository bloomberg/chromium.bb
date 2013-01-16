// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/frame_rate_counter.h"

#include <limits>

#include "base/metrics/histogram.h"
#include "cc/proxy.h"

namespace cc {

const double FrameRateCounter::kFrameTooFast = 1.0 / 70.0; // measured in seconds
const double FrameRateCounter::kFrameTooSlow = 1.0 / 4.0;
const double FrameRateCounter::kDroppedFrameTime = 1.0 / 50.0;

// static
scoped_ptr<FrameRateCounter> FrameRateCounter::create(bool hasImplThread) {
  return make_scoped_ptr(new FrameRateCounter(hasImplThread));
}

inline base::TimeDelta FrameRateCounter::recentFrameInterval(size_t n) const
{
    DCHECK(n > 0);
    return m_ringBuffer.ReadBuffer(n) - m_ringBuffer.ReadBuffer(n - 1);
}

FrameRateCounter::FrameRateCounter(bool hasImplThread)
    : m_hasImplThread(hasImplThread)
    , m_droppedFrameCount(0)
{
}

void FrameRateCounter::saveTimeStamp(base::TimeTicks timestamp)
{
    m_ringBuffer.SaveToBuffer(timestamp);

    // Check if frame interval can be computed.
    if (m_ringBuffer.CurrentIndex() < 2)
        return;

    base::TimeDelta frameIntervalSeconds = recentFrameInterval(m_ringBuffer.BufferSize() - 1);

    if (m_hasImplThread && m_ringBuffer.CurrentIndex() > 0)
        HISTOGRAM_CUSTOM_COUNTS("Renderer4.CompositorThreadImplDrawDelay", frameIntervalSeconds.InMilliseconds(), 1, 120, 60);

    if (!isBadFrameInterval(frameIntervalSeconds) &&
        frameIntervalSeconds.InSecondsF() > kDroppedFrameTime)
        ++m_droppedFrameCount;
}

bool FrameRateCounter::isBadFrameInterval(base::TimeDelta intervalBetweenConsecutiveFrames) const
{
    double delta = intervalBetweenConsecutiveFrames.InSecondsF();
    bool schedulerAllowsDoubleFrames = !m_hasImplThread;
    bool intervalTooFast = schedulerAllowsDoubleFrames ? delta < kFrameTooFast : delta <= 0.0;
    bool intervalTooSlow = delta > kFrameTooSlow;
    return intervalTooFast || intervalTooSlow;
}

void FrameRateCounter::getMinAndMaxFPS(double& minFPS, double& maxFPS) const
{
    minFPS = std::numeric_limits<double>::max();
    maxFPS = 0;

    for (size_t i = m_ringBuffer.BufferSize() - 1; i > 0 && m_ringBuffer.IsFilledIndex(i - 1); --i) {
        base::TimeDelta delta = recentFrameInterval(i);

        if (isBadFrameInterval(delta))
            continue;

        DCHECK(delta.InSecondsF() > 0);
        double fps = 1.0 / delta.InSecondsF();

        minFPS = std::min(fps, minFPS);
        maxFPS = std::max(fps, maxFPS);
    }

    if (minFPS > maxFPS)
        minFPS = maxFPS;
}

double FrameRateCounter::getAverageFPS() const
{
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

    for (size_t i = m_ringBuffer.BufferSize() - 1; i > 0 && m_ringBuffer.IsFilledIndex(i - 1) && frameTimesTotal < 1.0; --i) {
        base::TimeDelta delta = recentFrameInterval(i);

        if (!isBadFrameInterval(delta)) {
            frameCount++;
            frameTimesTotal += delta.InSecondsF();
        } else if (frameCount)
            break;
    }

    if (frameCount) {
        DCHECK(frameTimesTotal > 0);
        averageFPS = frameCount / frameTimesTotal;
    }

    return averageFPS;
}

base::TimeTicks FrameRateCounter::timeStampOfRecentFrame(size_t n) const
{
    DCHECK(n < m_ringBuffer.BufferSize());

    if (m_ringBuffer.IsFilledIndex(n))
        return m_ringBuffer.ReadBuffer(n);

    return base::TimeTicks();
}

}  // namespace cc
