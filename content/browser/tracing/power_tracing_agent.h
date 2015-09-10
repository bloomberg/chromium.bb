// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_POWER_TRACING_AGENT_H_
#define CONTENT_BROWSER_TRACING_POWER_TRACING_AGENT_H_

#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread.h"

namespace base {
template <typename Type>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {

class BattorPowerTraceProvider;

class PowerTracingAgent {
 public:
  typedef base::Callback<void(const scoped_refptr<base::RefCountedString>&)>
      OutputCallback;

  bool StartTracing();
  void StopTracing(const OutputCallback& callback);

  // Retrieve the singleton instance.
  static PowerTracingAgent* GetInstance();

 private:
  // This allows constructor and destructor to be private and usable only
  // by the Singleton class.
  friend struct base::DefaultSingletonTraits<PowerTracingAgent>;

  // Constructor.
  PowerTracingAgent();
  virtual ~PowerTracingAgent();

  void OnStopTracingDone(const OutputCallback& callback,
                         const scoped_refptr<base::RefCountedString>& result);

  void FlushOnThread(const OutputCallback& callback);

  scoped_ptr<BattorPowerTraceProvider> battor_trace_provider_;
  bool is_tracing_;

  DISALLOW_COPY_AND_ASSIGN(PowerTracingAgent);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_POWER_TRACING_AGENT_H_
