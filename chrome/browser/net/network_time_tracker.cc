// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_time_tracker.h"

#include "base/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"

namespace {

// Helper functions for interacting with the NetworkTimeNotifier.
// Registration happens as follows (assuming tracker lives on thread N):
// | Thread N |                   | UI thread|                     | IO Thread |
// Start
//                         RegisterObserverOnUIThread
//                                                    RegisterObserverOnIOThread
//                                              NetworkTimeNotifier::AddObserver
// after which updates to the notifier and the subsequent observer calls
// happen as follows (assuming the network time update comes from the same
// thread):
// | Thread N |                   | UI thread|                     | IO Thread |
// UpdateNetworkNotifier
//                                               UpdateNetworkNotifierOnIOThread
//                                        NetworkTimeNotifier::UpdateNetworkTime
//                                                OnNetworkTimeUpdatedOnIOThread
// OnNetworkTimeUpdated
void RegisterObserverOnIOThread(
    IOThread* io_thread,
    const net::NetworkTimeNotifier::ObserverCallback& observer_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  io_thread->globals()->network_time_notifier->AddObserver(observer_callback);
}

void RegisterObserverOnUIThread(
    const net::NetworkTimeNotifier::ObserverCallback& observer_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RegisterObserverOnIOThread,
                 g_browser_process->io_thread(),
                 observer_callback));
}

void UpdateNetworkNotifierOnIOThread(IOThread* io_thread,
                                     const base::Time& network_time,
                                     const base::TimeDelta& resolution,
                                     const base::TimeDelta& latency,
                                     const base::TimeTicks& post_time) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  io_thread->globals()->network_time_notifier->UpdateNetworkTime(
      network_time, resolution, latency, post_time);
}

void UpdateNetworkNotifier(IOThread* io_thread,
                           const base::Time& network_time,
                           const base::TimeDelta& resolution,
                           const base::TimeDelta& latency) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&UpdateNetworkNotifierOnIOThread,
                 io_thread,
                 network_time,
                 resolution,
                 latency,
                 base::TimeTicks::Now()));
}

void OnNetworkTimeUpdatedOnIOThread(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const net::NetworkTimeNotifier::ObserverCallback& observer_callback,
    const base::Time& network_time,
    const base::TimeTicks& network_time_ticks,
    const base::TimeDelta& network_time_uncertainty) {
  task_runner->PostTask(
      FROM_HERE,
      base::Bind(observer_callback,
                 network_time,
                 network_time_ticks,
                 network_time_uncertainty));
}

}  // namespace

NetworkTimeTracker::NetworkTimeTracker()
    : weak_ptr_factory_(this) {
}

NetworkTimeTracker::~NetworkTimeTracker() {
}

void NetworkTimeTracker::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&RegisterObserverOnUIThread,
                 BuildObserverCallback()));
}

bool NetworkTimeTracker::GetNetworkTime(const base::TimeTicks& time_ticks,
                                        base::Time* network_time,
                                        base::TimeDelta* uncertainty) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(network_time);
  if (network_time_.is_null())
    return false;
  DCHECK(!network_time_ticks_.is_null());
  *network_time = network_time_ + (time_ticks - network_time_ticks_);
  if (uncertainty)
    *uncertainty = network_time_uncertainty_;
  return true;
}

// static
// Note: UpdateNetworkNotifier is exposed via callback because getting the IO
// thread pointer must be done on the UI thread, while components that provide
// network time updates may live on other threads.
NetworkTimeTracker::UpdateCallback
NetworkTimeTracker::BuildNotifierUpdateCallback() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return base::Bind(&UpdateNetworkNotifier,
                    g_browser_process->io_thread());
}

net::NetworkTimeNotifier::ObserverCallback
NetworkTimeTracker::BuildObserverCallback() {
  return base::Bind(&OnNetworkTimeUpdatedOnIOThread,
                    MessageLoop::current()->message_loop_proxy(),
                    base::Bind(&NetworkTimeTracker::OnNetworkTimeUpdate,
                               weak_ptr_factory_.GetWeakPtr()));
}

void NetworkTimeTracker::OnNetworkTimeUpdate(
    const base::Time& network_time,
    const base::TimeTicks& network_time_ticks,
    const base::TimeDelta& network_time_uncertainty) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_time_ = network_time;
  network_time_ticks_ = network_time_ticks;
  network_time_uncertainty_ = network_time_uncertainty;
}

