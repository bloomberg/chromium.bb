// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_ROUTING_DELEGATE_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_ROUTING_DELEGATE_H_
#pragma once

#include "base/basictypes.h"

class RenderViewHost;

// Interface for registering RenderViewHost instances for resource routing
// automation.
class AutomationResourceRoutingDelegate {
 public:
  // Call to register |render_view_host| for resource routing automation
  // by the delegate.
  virtual void RegisterRenderViewHost(RenderViewHost* render_view_host);

  // Call to unregister |render_view_host| from resource routing automation.
  virtual void UnregisterRenderViewHost(RenderViewHost* render_view_host);

 protected:
  AutomationResourceRoutingDelegate();
  virtual ~AutomationResourceRoutingDelegate();

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomationResourceRoutingDelegate);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_RESOURCE_ROUTING_DELEGATE_H_
