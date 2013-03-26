// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_RATE_LIMITER_H_
#define CC_SCHEDULER_RATE_LIMITER_H_

#include "base/memory/ref_counted.h"

namespace WebKit { class WebGraphicsContext3D; }

namespace cc {

class Thread;

class RateLimiterClient {
 public:
  virtual void RateLimit() = 0;

 protected:
  virtual ~RateLimiterClient() {}
};

// A RateLimiter can be used to make sure that a single context does not
// dominate all execution time.  To use, construct a RateLimiter class around
// the context and call Start() whenever calls are made on the context outside
// of normal flow control. RateLimiter will block if the context is too far
// ahead of the compositor.
class RateLimiter : public base::RefCounted<RateLimiter> {
 public:
  static scoped_refptr<RateLimiter> Create(
      WebKit::WebGraphicsContext3D* context,
      RateLimiterClient* client,
      Thread* thread);

  void Start();

  // Context and client will not be accessed after Stop().
  void Stop();

 private:
  friend class base::RefCounted<RateLimiter>;

  RateLimiter(WebKit::WebGraphicsContext3D* context,
              RateLimiterClient* client,
              Thread* thread);
  ~RateLimiter();

  void RateLimitContext();

  WebKit::WebGraphicsContext3D* context_;
  bool active_;
  RateLimiterClient* client_;
  Thread* thread_;

  DISALLOW_COPY_AND_ASSIGN(RateLimiter);
};

}  // namespace cc

#endif  // CC_SCHEDULER_RATE_LIMITER_H_
