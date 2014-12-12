// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

class DevToolsAgentHost;
class RenderViewHost;
class WebContents;

// This class is a singleton that manage global DevTools state for the whole
// browser.
class CONTENT_EXPORT DevToolsManager {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    typedef DevToolsManagerDelegate::TargetList TargetList;

    // Called when any target information changed. Targets in the list are
    // owned by DevToolsManager, so they should not be accessed outside of
    // this method.
    virtual void TargetListChanged(const TargetList& targets) {}
  };

  // Returns single instance of this class. The instance is destroyed on the
  // browser main loop exit so this method MUST NOT be called after that point.
  static DevToolsManager* GetInstance();

  DevToolsManager();
  virtual ~DevToolsManager();

  DevToolsManagerDelegate* delegate() const { return delegate_.get(); }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void RenderViewCreated(WebContents* web_contents, RenderViewHost* rvh);
  void AgentHostChanged(scoped_refptr<DevToolsAgentHost> agent_host);

  typedef base::Callback<void(base::Closure)> Scheduler;
  void SetUpForTest(Scheduler scheduler);

 private:
  friend struct DefaultSingletonTraits<DevToolsManager>;

  void UpdateTargetList();
  void UpdateTargetListThrottled();
  void NotifyTargetListChanged(const Observer::TargetList& targets);

  scoped_ptr<DevToolsManagerDelegate> delegate_;
  ObserverList<Observer> observer_list_;
  std::set<DevToolsAgentHost*> attached_hosts_;
  bool update_target_list_required_;
  bool update_target_list_scheduled_;
  base::CancelableClosure update_target_list_callback_;
  Scheduler scheduler_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
