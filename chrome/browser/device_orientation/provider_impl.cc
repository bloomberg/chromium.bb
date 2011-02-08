// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <set>
#include <vector>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/device_orientation/orientation.h"
#include "chrome/browser/device_orientation/provider_impl.h"

namespace device_orientation {

ProviderImpl::ProviderImpl(const DataFetcherFactory factories[])
    : creator_loop_(MessageLoop::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(do_poll_method_factory_(this)) {
  for (const DataFetcherFactory* fp = factories; *fp; ++fp)
    factories_.push_back(*fp);
}

ProviderImpl::~ProviderImpl() {
}

void ProviderImpl::AddObserver(Observer* observer) {
  DCHECK(MessageLoop::current() == creator_loop_);

  observers_.insert(observer);
  if (observers_.size() == 1)
    Start();
  else
    observer->OnOrientationUpdate(last_notification_);
}

void ProviderImpl::RemoveObserver(Observer* observer) {
  DCHECK(MessageLoop::current() == creator_loop_);

  observers_.erase(observer);
  if (observers_.empty())
    Stop();
}

void ProviderImpl::Start() {
  DCHECK(MessageLoop::current() == creator_loop_);
  DCHECK(!polling_thread_.get());

  polling_thread_.reset(new base::Thread("Device orientation polling thread"));
  if (!polling_thread_->Start()) {
    LOG(ERROR) << "Failed to start device orientation polling thread";
    polling_thread_.reset();
    return;
  }
  ScheduleInitializePollingThread();
}

void ProviderImpl::Stop() {
  DCHECK(MessageLoop::current() == creator_loop_);

  // TODO(hans): Don't join the thread. See crbug.com/72286.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  polling_thread_.reset();
  data_fetcher_.reset();
}

void ProviderImpl::DoInitializePollingThread(
    std::vector<DataFetcherFactory> factories) {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());

  typedef std::vector<DataFetcherFactory>::const_iterator Iterator;
  for (Iterator i = factories_.begin(), e = factories_.end(); i != e; ++i) {
    DataFetcherFactory factory = *i;
    scoped_ptr<DataFetcher> fetcher(factory());
    Orientation orientation;

    if (fetcher.get() && fetcher->GetOrientation(&orientation)) {
      // Pass ownership of fetcher to provider_.
      data_fetcher_.swap(fetcher);
      last_orientation_ = orientation;

      // Notify observers.
      ScheduleDoNotify(orientation);

      // Start polling.
      ScheduleDoPoll();
      return;
    }
  }

  // When no orientation data can be provided.
  ScheduleDoNotify(Orientation::Empty());
}

void ProviderImpl::ScheduleInitializePollingThread() {
  DCHECK(MessageLoop::current() == creator_loop_);

  Task* task = NewRunnableMethod(this,
                                 &ProviderImpl::DoInitializePollingThread,
                                 factories_);
  MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostTask(FROM_HERE, task);
}

void ProviderImpl::DoNotify(const Orientation& orientation) {
  DCHECK(MessageLoop::current() == creator_loop_);

  last_notification_ = orientation;

  typedef std::set<Observer*>::const_iterator Iterator;
  for (Iterator i = observers_.begin(), e = observers_.end(); i != e; ++i)
    (*i)->OnOrientationUpdate(orientation);

  if (orientation.IsEmpty()) {
    // Notify observers about failure to provide data exactly once.
    observers_.clear();
    Stop();
  }
}

void ProviderImpl::ScheduleDoNotify(const Orientation& orientation) {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());

  Task* task = NewRunnableMethod(this, &ProviderImpl::DoNotify, orientation);
  creator_loop_->PostTask(FROM_HERE, task);
}

void ProviderImpl::DoPoll() {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());

  Orientation orientation;
  if (!data_fetcher_->GetOrientation(&orientation)) {
    LOG(ERROR) << "Failed to poll device orientation data fetcher.";

    ScheduleDoNotify(Orientation::Empty());
    return;
  }

  if (SignificantlyDifferent(orientation, last_orientation_)) {
    last_orientation_ = orientation;
    ScheduleDoNotify(orientation);
  }

  ScheduleDoPoll();
}

void ProviderImpl::ScheduleDoPoll() {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());

  Task* task = do_poll_method_factory_.NewRunnableMethod(&ProviderImpl::DoPoll);
  MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostDelayedTask(FROM_HERE, task, SamplingIntervalMs());
}

namespace {

bool IsElementSignificantlyDifferent(bool can_provide_element1,
                                     bool can_provide_element2,
                                     double element1,
                                     double element2) {
  const double kThreshold = 0.1;

  if (can_provide_element1 != can_provide_element2)
    return true;
  if (can_provide_element1 &&
      std::fabs(element1 - element2) >= kThreshold)
    return true;
  return false;
}
}  // namespace

// Returns true if two orientations are considered different enough that
// observers should be notified of the new orientation.
bool ProviderImpl::SignificantlyDifferent(const Orientation& o1,
                                          const Orientation& o2) {
  return IsElementSignificantlyDifferent(o1.can_provide_alpha_,
                                         o2.can_provide_alpha_,
                                         o1.alpha_,
                                         o2.alpha_) ||
      IsElementSignificantlyDifferent(o1.can_provide_beta_,
                                         o2.can_provide_beta_,
                                         o1.beta_,
                                         o2.beta_) ||
      IsElementSignificantlyDifferent(o1.can_provide_gamma_,
                                         o2.can_provide_gamma_,
                                         o1.gamma_,
                                         o2.gamma_);
}

int ProviderImpl::SamplingIntervalMs() const {
  DCHECK(MessageLoop::current() == polling_thread_->message_loop());
  DCHECK(data_fetcher_.get());

  // TODO(erg): There used to be unused code here, that called a default
  // implementation on the DataFetcherInterface that was never defined. I'm
  // removing unused methods from headers.
  return kDesiredSamplingIntervalMs;
}

}  // namespace device_orientation
