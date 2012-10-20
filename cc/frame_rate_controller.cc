// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCFrameRateController.h"

#include "CCDelayBasedTimeSource.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cc/time_source.h"
#include <wtf/CurrentTime.h>

namespace {

// This will be the maximum number of pending frames unless
// CCFrameRateController::setMaxFramesPending is called.
const int defaultMaxFramesPending = 2;

}  // namespace

namespace cc {

class CCFrameRateControllerTimeSourceAdapter : public CCTimeSourceClient {
public:
    static scoped_ptr<CCFrameRateControllerTimeSourceAdapter> create(CCFrameRateController* frameRateController) {
        return make_scoped_ptr(new CCFrameRateControllerTimeSourceAdapter(frameRateController));
    }
    virtual ~CCFrameRateControllerTimeSourceAdapter() {}

    virtual void onTimerTick() OVERRIDE {
      m_frameRateController->onTimerTick();
    }

private:
    explicit CCFrameRateControllerTimeSourceAdapter(CCFrameRateController* frameRateController)
        : m_frameRateController(frameRateController) {}

    CCFrameRateController* m_frameRateController;
};

CCFrameRateController::CCFrameRateController(scoped_refptr<CCTimeSource> timer)
    : m_client(0)
    , m_numFramesPending(0)
    , m_maxFramesPending(defaultMaxFramesPending)
    , m_timeSource(timer)
    , m_active(false)
    , m_swapBuffersCompleteSupported(true)
    , m_isTimeSourceThrottling(true)
{
    m_timeSourceClientAdapter = CCFrameRateControllerTimeSourceAdapter::create(this);
    m_timeSource->setClient(m_timeSourceClientAdapter.get());
}

CCFrameRateController::CCFrameRateController(CCThread* thread)
    : m_client(0)
    , m_numFramesPending(0)
    , m_maxFramesPending(defaultMaxFramesPending)
    , m_active(false)
    , m_swapBuffersCompleteSupported(true)
    , m_isTimeSourceThrottling(false)
    , m_manualTicker(new CCTimer(thread, this))
{
}

CCFrameRateController::~CCFrameRateController()
{
    if (m_isTimeSourceThrottling)
        m_timeSource->setActive(false);
}

void CCFrameRateController::setActive(bool active)
{
    if (m_active == active)
        return;
    TRACE_EVENT1("cc", "CCFrameRateController::setActive", "active", active);
    m_active = active;

    if (m_isTimeSourceThrottling)
        m_timeSource->setActive(active);
    else {
        if (active)
            postManualTick();
        else
            m_manualTicker->stop();
    }
}

void CCFrameRateController::setMaxFramesPending(int maxFramesPending)
{
    DCHECK(maxFramesPending > 0);
    m_maxFramesPending = maxFramesPending;
}

void CCFrameRateController::setTimebaseAndInterval(base::TimeTicks timebase, base::TimeDelta interval)
{
    if (m_isTimeSourceThrottling)
        m_timeSource->setTimebaseAndInterval(timebase, interval);
}

void CCFrameRateController::setSwapBuffersCompleteSupported(bool supported)
{
    m_swapBuffersCompleteSupported = supported;
}

void CCFrameRateController::onTimerTick()
{
    DCHECK(m_active);

    // Check if we have too many frames in flight.
    bool throttled = m_numFramesPending >= m_maxFramesPending;

    if (m_client)
        m_client->vsyncTick(throttled);

    if (m_swapBuffersCompleteSupported && !m_isTimeSourceThrottling && m_numFramesPending < m_maxFramesPending)
        postManualTick();
}

void CCFrameRateController::postManualTick()
{
    if (m_active)
        m_manualTicker->startOneShot(0);
}

void CCFrameRateController::onTimerFired()
{
    onTimerTick();
}

void CCFrameRateController::didBeginFrame()
{
    if (m_swapBuffersCompleteSupported)
        m_numFramesPending++;
    else if (!m_isTimeSourceThrottling)
        postManualTick();
}

void CCFrameRateController::didFinishFrame()
{
    DCHECK(m_swapBuffersCompleteSupported);

    m_numFramesPending--;
    if (!m_isTimeSourceThrottling)
        postManualTick();
}

void CCFrameRateController::didAbortAllPendingFrames()
{
    m_numFramesPending = 0;
}

base::TimeTicks CCFrameRateController::nextTickTime()
{
    if (m_isTimeSourceThrottling)
        return m_timeSource->nextTickTime();

    return base::TimeTicks();
}

}
