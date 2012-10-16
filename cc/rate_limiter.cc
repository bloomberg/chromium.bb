// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/rate_limiter.h"

#include "cc/proxy.h"
#include "cc/thread.h"
#include "TraceEvent.h"
#include <public/WebGraphicsContext3D.h>
#include <wtf/RefPtr.h>

namespace cc {

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

    scoped_refptr<RateLimiter> m_rateLimiter;
};

scoped_refptr<RateLimiter> RateLimiter::create(WebKit::WebGraphicsContext3D* context, RateLimiterClient *client)
{
    return make_scoped_refptr(new RateLimiter(context, client));
}

RateLimiter::RateLimiter(WebKit::WebGraphicsContext3D* context, RateLimiterClient *client)
    : m_context(context)
    , m_active(false)
    , m_client(client)
{
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
