// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "content/browser/device_orientation/orientation.h"
#include "content/browser/device_orientation/provider_impl.h"

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

void DeleteThread(base::Thread* thread) {
  thread->Stop();
  delete thread;
}

}

namespace device_orientation {

class ProviderImpl::PollingThread : public base::Thread {
 public:
  PollingThread(const char* name,
                base::WeakPtr<ProviderImpl> provider,
                MessageLoop* creator_loop);
  virtual ~PollingThread();

  // Method for finding a suitable DataFetcher and starting the polling.
  void Initialize(const std::vector<DataFetcherFactory>& factories);

 private:
  // Method for polling a DataFetcher.
  void DoPoll();
  void ScheduleDoPoll();

  // Schedule a notification to the |provider_| which lives on a different
  // thread (|creator_loop_| is its message loop).
  void ScheduleDoNotify(const Orientation& orientation);

  static bool SignificantlyDifferent(const Orientation& orientation1,
                                     const Orientation& orientation2);

  enum { kDesiredSamplingIntervalMs = 100 };
  base::TimeDelta SamplingInterval() const;

  // The Message Loop on which this object was created.
  // Typically the I/O loop, but may be something else during testing.
  MessageLoop* creator_loop_;

  scoped_ptr<DataFetcher> data_fetcher_;
  Orientation last_orientation_;

  base::WeakPtr<ProviderImpl> provider_;
};

ProviderImpl::PollingThread::PollingThread(
    const char* name,
    base::WeakPtr<ProviderImpl> provider,
    MessageLoop* creator_loop)
    : base::Thread(name),
      creator_loop_(creator_loop),
      provider_(provider) {
}

ProviderImpl::PollingThread::~PollingThread() {
}

void ProviderImpl::PollingThread::Initialize(
    const std::vector<DataFetcherFactory>& factories) {
  DCHECK(MessageLoop::current() == message_loop());

  typedef std::vector<DataFetcherFactory>::const_iterator Iterator;
  for (Iterator i = factories.begin(); i != factories.end(); ++i) {
    DataFetcherFactory factory = *i;
    scoped_ptr<DataFetcher> fetcher(factory());
    Orientation orientation;

    if (fetcher.get() && fetcher->GetOrientation(&orientation)) {
      // Pass ownership of fetcher to provider_.
      data_fetcher_.swap(fetcher);
      last_orientation_ = orientation;

      // Notify observers.
      if (!orientation.is_empty())
        ScheduleDoNotify(orientation);

      // Start polling.
      ScheduleDoPoll();
      return;
    }
  }

  // When no orientation data can be provided.
  ScheduleDoNotify(Orientation::Empty());
}

void ProviderImpl::PollingThread::ScheduleDoNotify(
    const Orientation& orientation) {
  DCHECK(MessageLoop::current() == message_loop());

  creator_loop_->PostTask(
      FROM_HERE, base::Bind(&ProviderImpl::DoNotify, provider_, orientation));
}

void ProviderImpl::PollingThread::DoPoll() {
  DCHECK(MessageLoop::current() == message_loop());

  Orientation orientation;
  if (!data_fetcher_->GetOrientation(&orientation)) {
    LOG(ERROR) << "Failed to poll device orientation data fetcher.";

    ScheduleDoNotify(Orientation::Empty());
    return;
  }

  if (!orientation.is_empty() &&
      SignificantlyDifferent(orientation, last_orientation_)) {
    last_orientation_ = orientation;
    ScheduleDoNotify(orientation);
  }

  ScheduleDoPoll();
}

void ProviderImpl::PollingThread::ScheduleDoPoll() {
  DCHECK(MessageLoop::current() == message_loop());

  message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PollingThread::DoPoll, base::Unretained(this)),
      SamplingInterval());
}

// Returns true if two orientations are considered different enough that
// observers should be notified of the new orientation.
bool ProviderImpl::PollingThread::SignificantlyDifferent(
    const Orientation& o1,
    const Orientation& o2) {
  return IsElementSignificantlyDifferent(o1.can_provide_alpha(),
                                         o2.can_provide_alpha(),
                                         o1.alpha(),
                                         o2.alpha()) ||
         IsElementSignificantlyDifferent(o1.can_provide_beta(),
                                         o2.can_provide_beta(),
                                         o1.beta(),
                                         o2.beta()) ||
         IsElementSignificantlyDifferent(o1.can_provide_gamma(),
                                         o2.can_provide_gamma(),
                                         o1.gamma(),
                                         o2.gamma()) ||
         (o1.can_provide_absolute() != o2.can_provide_absolute() ||
          o1.absolute() != o2.absolute());
}

base::TimeDelta ProviderImpl::PollingThread::SamplingInterval() const {
  DCHECK(MessageLoop::current() == message_loop());
  DCHECK(data_fetcher_.get());

  // TODO(erg): There used to be unused code here, that called a default
  // implementation on the DataFetcherInterface that was never defined. I'm
  // removing unused methods from headers.
  return base::TimeDelta::FromMilliseconds(kDesiredSamplingIntervalMs);
}

ProviderImpl::ProviderImpl(const DataFetcherFactory factories[])
    : creator_loop_(MessageLoop::current()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      polling_thread_(NULL) {
  for (const DataFetcherFactory* fp = factories; *fp; ++fp)
    factories_.push_back(*fp);
}

ProviderImpl::~ProviderImpl() {
  Stop();
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
  DCHECK(!polling_thread_);

  polling_thread_ = new PollingThread("Device orientation polling thread",
                                      weak_factory_.GetWeakPtr(),
                                      creator_loop_);
  if (!polling_thread_->Start()) {
    LOG(ERROR) << "Failed to start device orientation polling thread";
    delete polling_thread_;
    polling_thread_ = NULL;
    return;
  }
  ScheduleInitializePollingThread();
}

void ProviderImpl::Stop() {
  DCHECK(MessageLoop::current() == creator_loop_);

  weak_factory_.InvalidateWeakPtrs();
  if (polling_thread_) {
    polling_thread_->StopSoon();
    bool posted = base::WorkerPool::PostTask(
        FROM_HERE,
        base::Bind(&DeleteThread, base::Unretained(polling_thread_)),
        true /* task is slow */);
    DCHECK(posted);
    polling_thread_ = NULL;
  }
}

void ProviderImpl::ScheduleInitializePollingThread() {
  DCHECK(MessageLoop::current() == creator_loop_);

  MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostTask(FROM_HERE,
                         base::Bind(&PollingThread::Initialize,
                                    base::Unretained(polling_thread_),
                                    factories_));
}

void ProviderImpl::DoNotify(const Orientation& orientation) {
  DCHECK(MessageLoop::current() == creator_loop_);

  last_notification_ = orientation;

  typedef std::set<Observer*>::const_iterator Iterator;
  for (Iterator i = observers_.begin(); i != observers_.end(); ++i)
    (*i)->OnOrientationUpdate(orientation);

  if (orientation.is_empty()) {
    // Notify observers about failure to provide data exactly once.
    observers_.clear();
    Stop();
  }
}

}  // namespace device_orientation
