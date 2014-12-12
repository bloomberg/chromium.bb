// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_manager.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_netlog_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_target.h"

namespace content {

namespace {

int kObserverThrottleInterval = 500; // ms

}  // namespace

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return Singleton<DevToolsManager>::get();
}

DevToolsManager::DevToolsManager()
    : delegate_(GetContentClient()->browser()->GetDevToolsManagerDelegate()),
      update_target_list_required_(false),
      update_target_list_scheduled_(false),
      update_target_list_callback_(base::Bind(
          &DevToolsManager::UpdateTargetListThrottled,
          base::Unretained(this))) {
}

DevToolsManager::~DevToolsManager() {
  DCHECK(!attached_hosts_.size());
  update_target_list_callback_.Cancel();
}

void DevToolsManager::RenderViewCreated(
    WebContents* web_contents, RenderViewHost* rvh) {
  if (observer_list_.might_have_observers()) {
    // Force agent host creation.
    DevToolsAgentHost::GetOrCreateFor(web_contents);
  }
}

void DevToolsManager::AgentHostChanged(
    scoped_refptr<DevToolsAgentHost> agent_host) {
  bool was_attached =
      attached_hosts_.find(agent_host.get()) != attached_hosts_.end();
  if (agent_host->IsAttached() && !was_attached) {
    if (!attached_hosts_.size()) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&DevToolsNetLogObserver::Attach));
    }
    attached_hosts_.insert(agent_host.get());
  } else if (!agent_host->IsAttached() && was_attached) {
    attached_hosts_.erase(agent_host.get());
    if (!attached_hosts_.size()) {
      BrowserThread::PostTask(
          BrowserThread::IO,
          FROM_HERE,
          base::Bind(&DevToolsNetLogObserver::Detach));
    }
  }

  UpdateTargetList();
}

void DevToolsManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
  UpdateTargetList();
}

void DevToolsManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DevToolsManager::UpdateTargetList() {
  if (!observer_list_.might_have_observers())
    return;

  update_target_list_required_ = true;
  if (!update_target_list_scheduled_)
    UpdateTargetListThrottled();
}

void DevToolsManager::UpdateTargetListThrottled() {
  if (!update_target_list_required_) {
    update_target_list_scheduled_ = false;
    return;
  }

  update_target_list_scheduled_ = true;
  if (scheduler_.is_null()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        update_target_list_callback_.callback(),
        base::TimeDelta::FromMilliseconds(kObserverThrottleInterval));
  } else {
    scheduler_.Run(update_target_list_callback_.callback());
  }

  update_target_list_required_ = false;
  if (!delegate_) {
    Observer::TargetList empty_list;
    NotifyTargetListChanged(empty_list);
    return;
  }
  delegate_->EnumerateTargets(base::Bind(
      &DevToolsManager::NotifyTargetListChanged,
      base::Unretained(this)));
}

void DevToolsManager::NotifyTargetListChanged(
    const Observer::TargetList& targets) {
  FOR_EACH_OBSERVER(Observer, observer_list_, TargetListChanged(targets));
  STLDeleteContainerPointers(targets.begin(), targets.end());
}

void DevToolsManager::SetUpForTest(Scheduler scheduler) {
  scheduler_ = scheduler;
  delegate_.reset(GetContentClient()->browser()->GetDevToolsManagerDelegate());
}

}  // namespace content
