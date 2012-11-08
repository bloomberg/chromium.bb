// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RATE_LIMITER_H_
#define CC_RATE_LIMITER_H_

#include "base/memory/ref_counted.h"

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class Thread;

class RateLimiterClient {
public:
    virtual void rateLimit() = 0;
};

// A RateLimiter can be used to make sure that a single context does not dominate all execution time.
// To use, construct a RateLimiter class around the context and call start() whenever calls are made on the
// context outside of normal flow control. RateLimiter will block if the context is too far ahead of the
// compositor.
class RateLimiter : public base::RefCounted<RateLimiter> {
public:
    static scoped_refptr<RateLimiter> create(WebKit::WebGraphicsContext3D*, RateLimiterClient*, Thread*);

    void start();

    // Context and client will not be accessed after stop().
    void stop();

private:
    RateLimiter(WebKit::WebGraphicsContext3D*, RateLimiterClient*, Thread*);
    ~RateLimiter();
    friend class base::RefCounted<RateLimiter>;

    class Task;
    friend class Task;
    void rateLimitContext();

    WebKit::WebGraphicsContext3D* m_context;
    bool m_active;
    RateLimiterClient *m_client;
    Thread* m_thread;
};

}
#endif  // CC_RATE_LIMITER_H_
