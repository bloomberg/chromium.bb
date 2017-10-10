// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ARC_TRACING_AGENT_H_
#define CONTENT_PUBLIC_BROWSER_ARC_TRACING_AGENT_H_

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "content/common/content_export.h"
#include "services/resource_coordinator/public/interfaces/tracing/tracing.mojom.h"

namespace content {

// A wrapper for controlling tracing in ARC container.
// The overriden Stop/StartAgentTracing functions are implemented by the
// Delegate object to interact with ARC bridge. All the functions are expected
// to be called on the browser's UI thread.
class CONTENT_EXPORT ArcTracingAgent : public tracing::mojom::Agent {
 public:
  class Delegate {
   public:
    // Callback for StartTracing, takes one boolean argument to indicate
    // success or failure.
    using StartTracingCallback = base::Callback<void(bool)>;
    // Callback for StopTracing, takes one boolean argument to indicate
    // success or failure.
    using StopTracingCallback = base::Callback<void(bool)>;

    virtual ~Delegate();

    // Starts tracing in ARC container.
    virtual bool StartTracing(
        const base::trace_event::TraceConfig& trace_config,
        base::ScopedFD write_fd,
        const StartTracingCallback& callback) = 0;

    // Stops tracing in ARC container.
    virtual void StopTracing(const StopTracingCallback& callback) = 0;
  };

  ArcTracingAgent();
  ~ArcTracingAgent() override;

  static ArcTracingAgent* GetInstance();

  // Sets Delegate instance, which should implement the communication
  // using ARC bridge. If set to nullptr, calling of Start/StopAgentTracing
  // does nothing. Ownership of |delegate| remains with the caller.
  virtual void SetDelegate(Delegate* delegate) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcTracingAgent);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ARC_TRACING_AGENT_H_
