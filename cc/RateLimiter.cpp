// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "RateLimiter.h"

#include "CCProxy.h"
#include "CCThread.h"
#include "TraceEvent.h"
#include <public/WebGraphicsContext3D.h>

namespace WebCore {

class RateLimiter::Task : public CCThread::Task {
public:
    static PassOwnPtr<Task> create(RateLimiter* rateLimiter)
    {
        return adoptPtr(new Task(rateLimiter));
    }
    virtual ~Task() { }

private:
    explicit Task(RateLimiter* rateLimiter)
        : CCThread::Task(this)
        , m_rateLimiter(rateLimiter)
    {
    }

    virtual void performTask() OVERRIDE
    {
        m_rateLimiter->rateLimitContext();
    }

    RefPtr<RateLimiter> m_rateLimiter;
};

PassRefPtr<RateLimiter> RateLimiter::create(WebKit::WebGraphicsContext3D* context, RateLimiterClient *client)
{
    return adoptRef(new RateLimiter(context, client));
}

RateLimiter::RateLimiter(WebKit::WebGraphicsContext3D* context, RateLimiterClient *client)
    : m_context(context)
    , m_active(false)
    , m_client(client)
{
    turnOffVerifier();
    ASSERT(context);
}

RateLimiter::~RateLimiter()
{
}

void RateLimiter::start()
{
    if (m_active)
        return;

    TRACE_EVENT0("cc", "RateLimiter::start");
    m_active = true;
    CCProxy::mainThread()->postTask(RateLimiter::Task::create(this));
}

void RateLimiter::stop()
{
    TRACE_EVENT0("cc", "RateLimiter::stop");
    m_client = 0;
}

void RateLimiter::rateLimitContext()
{
    if (!m_client)
        return;

    TRACE_EVENT0("cc", "RateLimiter::rateLimitContext");

    m_active = false;
    m_client->rateLimit();
    m_context->rateLimitOffscreenContextCHROMIUM();
}

}
#endif // USE(ACCELERATED_COMPOSITING)
