// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/rate_limiter.h"

#include "base/debug/trace_event.h"
#include "cc/base/thread.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace cc {

scoped_refptr<RateLimiter> RateLimiter::Create(
    WebKit::WebGraphicsContext3D* context,
    RateLimiterClient* client,
    Thread* thread) {
  return make_scoped_refptr(new RateLimiter(context, client, thread));
}

RateLimiter::RateLimiter(WebKit::WebGraphicsContext3D* context,
                         RateLimiterClient* client,
                         Thread* thread)
    : context_(context),
      active_(false),
      client_(client),
      thread_(thread) {
  DCHECK(context);
}

RateLimiter::~RateLimiter() {}

void RateLimiter::Start() {
  if (active_)
    return;

  TRACE_EVENT0("cc", "RateLimiter::Start");
  active_ = true;
  thread_->PostTask(base::Bind(&RateLimiter::RateLimitContext, this));
}

void RateLimiter::Stop() {
  TRACE_EVENT0("cc", "RateLimiter::Stop");
  client_ = NULL;
}

void RateLimiter::RateLimitContext() {
  if (!client_)
    return;

  TRACE_EVENT0("cc", "RateLimiter::RateLimitContext");

  active_ = false;
  client_->RateLimit();
  context_->rateLimitOffscreenContextCHROMIUM();
}

}  // namespace cc
