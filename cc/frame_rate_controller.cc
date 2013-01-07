// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/frame_rate_controller.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cc/delay_based_time_source.h"
#include "cc/time_source.h"
#include "cc/thread.h"

namespace {

// This will be the maximum number of pending frames unless
// FrameRateController::setMaxFramesPending is called.
const int defaultMaxFramesPending = 2;

}  // namespace

namespace cc {

class FrameRateControllerTimeSourceAdapter : public TimeSourceClient {
public:
    static scoped_ptr<FrameRateControllerTimeSourceAdapter> create(FrameRateController* frameRateController) {
        return make_scoped_ptr(new FrameRateControllerTimeSourceAdapter(frameRateController));
    }
    virtual ~FrameRateControllerTimeSourceAdapter() {}

    virtual void onTimerTick() OVERRIDE {
      m_frameRateController->onTimerTick();
    }

private:
    explicit FrameRateControllerTimeSourceAdapter(FrameRateController* frameRateController)
        : m_frameRateController(frameRateController) {}

    FrameRateController* m_frameRateController;
};

FrameRateController::FrameRateController(scoped_refptr<TimeSource> timer)
    : m_client(0)
    , m_numFramesPending(0)
    , m_maxFramesPending(defaultMaxFramesPending)
    , m_timeSource(timer)
    , m_active(false)
    , m_swapBuffersCompleteSupported(true)
    , m_isTimeSourceThrottling(true)
    , m_thread(0)
    , m_weakFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this))
{
    m_timeSourceClientAdapter = FrameRateControllerTimeSourceAdapter::create(this);
    m_timeSource->setClient(m_timeSourceClientAdapter.get());
}

FrameRateController::FrameRateController(Thread* thread)
    : m_client(0)
    , m_numFramesPending(0)
    , m_maxFramesPending(defaultMaxFramesPending)
    , m_active(false)
    , m_swapBuffersCompleteSupported(true)
    , m_isTimeSourceThrottling(false)
    , m_thread(thread)
    , m_weakFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this))
{
}

FrameRateController::~FrameRateController()
{
    if (m_isTimeSourceThrottling)
        m_timeSource->setActive(false);
}

void FrameRateController::setActive(bool active)
{
    if (m_active == active)
        return;
    TRACE_EVENT1("cc", "FrameRateController::setActive", "active", active);
    m_active = active;

    if (m_isTimeSourceThrottling)
        m_timeSource->setActive(active);
    else {
        if (active)
            postManualTick();
        else
            m_weakFactory.InvalidateWeakPtrs();
    }
}

void FrameRateController::setMaxFramesPending(int maxFramesPending)
{
    DCHECK(maxFramesPending > 0);
    m_maxFramesPending = maxFramesPending;
}

void FrameRateController::setTimebaseAndInterval(base::TimeTicks timebase, base::TimeDelta interval)
{
    if (m_isTimeSourceThrottling)
        m_timeSource->setTimebaseAndInterval(timebase, interval);
}

void FrameRateController::setSwapBuffersCompleteSupported(bool supported)
{
    m_swapBuffersCompleteSupported = supported;
}

void FrameRateController::onTimerTick()
{
    DCHECK(m_active);

    // Check if we have too many frames in flight.
    bool throttled = m_numFramesPending >= m_maxFramesPending;
    TRACE_COUNTER_ID1("cc", "ThrottledVSyncInterval", m_thread, throttled);

    if (m_client)
        m_client->vsyncTick(throttled);

    if (m_swapBuffersCompleteSupported && !m_isTimeSourceThrottling && m_numFramesPending < m_maxFramesPending)
        postManualTick();
}

void FrameRateController::postManualTick()
{
    if (m_active)
        m_thread->postTask(base::Bind(&FrameRateController::manualTick, m_weakFactory.GetWeakPtr()));
}

void FrameRateController::manualTick()
{
    onTimerTick();
}

void FrameRateController::didBeginFrame()
{
    if (m_swapBuffersCompleteSupported)
        m_numFramesPending++;
    else if (!m_isTimeSourceThrottling)
        postManualTick();
}

void FrameRateController::didFinishFrame()
{
    DCHECK(m_swapBuffersCompleteSupported);

    m_numFramesPending--;
    if (!m_isTimeSourceThrottling)
        postManualTick();
}

void FrameRateController::didAbortAllPendingFrames()
{
    m_numFramesPending = 0;
}

base::TimeTicks FrameRateController::nextTickTime()
{
    if (m_isTimeSourceThrottling)
        return m_timeSource->nextTickTime();

    return base::TimeTicks();
}

}  // namespace cc
