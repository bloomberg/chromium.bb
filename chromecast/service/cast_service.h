// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SERVICE_CAST_SERVICE_H_
#define CHROMECAST_SERVICE_CAST_SERVICE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class ThreadChecker;
}

namespace content{
class BrowserContext;
}

namespace chromecast {

class CastService {
 public:
  static CastService* Create(content::BrowserContext* browser_context);

  virtual ~CastService();

  // Start/stop the cast service.
  void Start();
  void Stop();

 protected:
  explicit CastService(content::BrowserContext* browser_context);
  virtual void Initialize() = 0;

  // Implementation-specific start/stop behavior.
  virtual void StartInternal() = 0;
  virtual void StopInternal() = 0;

  content::BrowserContext* browser_context() const { return browser_context_; }

 private:
  content::BrowserContext* const browser_context_;
  bool stopped_;

  const scoped_ptr<base::ThreadChecker> thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(CastService);
};

}  // namespace chromecast

#endif  // CHROMECAST_SERVICE_CAST_SERVICE_H_
