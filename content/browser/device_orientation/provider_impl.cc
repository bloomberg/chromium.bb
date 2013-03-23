// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/provider_impl.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"

namespace {

void DeleteThread(base::Thread* thread) {
  delete thread;
}

}

namespace content {

class ProviderImpl::PollingThread : public base::Thread {
 public:
  PollingThread(const char* name,
                base::WeakPtr<ProviderImpl> provider,
                MessageLoop* creator_loop);
  virtual ~PollingThread();

  // Method for creating a DataFetcher and starting the polling, if the fetcher
  // can provide this type of data.
  void Initialize(DataFetcherFactory factory, DeviceData::Type type);

  // Method for adding a type of data to poll for.
  void DoAddPollingDataType(DeviceData::Type type);

 private:
  // Method for polling a DataFetcher.
  void DoPoll();
  void ScheduleDoPoll();

  // Schedule a notification to the |provider_| which lives on a different
  // thread (|creator_loop_| is its message loop).
  void ScheduleDoNotify(const scoped_refptr<const DeviceData>& device_data,
                        DeviceData::Type device_data_type);

  enum { kDesiredSamplingIntervalMs = 100 };
  base::TimeDelta SamplingInterval() const;

  // The Message Loop on which this object was created.
  // Typically the I/O loop, but may be something else during testing.
  MessageLoop* creator_loop_;

  scoped_ptr<DataFetcher> data_fetcher_;
  std::map<DeviceData::Type, scoped_refptr<const DeviceData> >
    last_device_data_map_;
  std::set<DeviceData::Type> polling_data_types_;

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
  Stop();
}

void ProviderImpl::PollingThread::DoAddPollingDataType(DeviceData::Type type) {
  DCHECK(MessageLoop::current() == message_loop());

  polling_data_types_.insert(type);
}

void ProviderImpl::PollingThread::Initialize(DataFetcherFactory factory,
                                             DeviceData::Type type) {
  DCHECK(MessageLoop::current() == message_loop());

  if (factory != NULL) {
    // Try to use factory to create a fetcher that can provide this type of
    // data. If factory creates a fetcher that provides this type of data,
    // start polling.
    scoped_ptr<DataFetcher> fetcher(factory());

    if (fetcher.get()) {
      scoped_refptr<const DeviceData> device_data(fetcher->GetDeviceData(type));
      if (device_data != NULL) {
        // Pass ownership of fetcher to provider_.
        data_fetcher_.swap(fetcher);
        last_device_data_map_[type] = device_data;

        // Notify observers.
        ScheduleDoNotify(device_data, type);

        // Start polling.
        ScheduleDoPoll();
        return;
      }
    }
  }

  // When no device data can be provided.
  ScheduleDoNotify(NULL, type);
}

void ProviderImpl::PollingThread::ScheduleDoNotify(
    const scoped_refptr<const DeviceData>& device_data,
    DeviceData::Type device_data_type) {
  DCHECK(MessageLoop::current() == message_loop());

  creator_loop_->PostTask(FROM_HERE,
                          base::Bind(&ProviderImpl::DoNotify, provider_,
                                     device_data, device_data_type));
}

void ProviderImpl::PollingThread::DoPoll() {
  DCHECK(MessageLoop::current() == message_loop());

  // Poll the fetcher for each type of data.
  typedef std::set<DeviceData::Type>::const_iterator SetIterator;
  for (SetIterator i = polling_data_types_.begin();
       i != polling_data_types_.end(); ++i) {
    DeviceData::Type device_data_type = *i;
    scoped_refptr<const DeviceData> device_data(data_fetcher_->GetDeviceData(
        device_data_type));

    if (device_data == NULL) {
      LOG(ERROR) << "Failed to poll device data fetcher.";
      ScheduleDoNotify(NULL, device_data_type);
      continue;
    }

    const DeviceData* old_data = last_device_data_map_[device_data_type];
    if (old_data != NULL && !device_data->ShouldFireEvent(old_data))
      continue;

    // Update the last device data of this type and notify observers.
    last_device_data_map_[device_data_type] = device_data;
    ScheduleDoNotify(device_data, device_data_type);
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

base::TimeDelta ProviderImpl::PollingThread::SamplingInterval() const {
  DCHECK(MessageLoop::current() == message_loop());
  DCHECK(data_fetcher_.get());

  // TODO(erg): There used to be unused code here, that called a default
  // implementation on the DataFetcherInterface that was never defined. I'm
  // removing unused methods from headers.
  return base::TimeDelta::FromMilliseconds(kDesiredSamplingIntervalMs);
}

ProviderImpl::ProviderImpl(DataFetcherFactory factory)
    : creator_loop_(MessageLoop::current()),
      factory_(factory),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      polling_thread_(NULL) {
}

ProviderImpl::~ProviderImpl() {
  Stop();
}

void ProviderImpl::ScheduleDoAddPollingDataType(DeviceData::Type type) {
  DCHECK(MessageLoop::current() == creator_loop_);

  MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostTask(FROM_HERE,
                         base::Bind(&PollingThread::DoAddPollingDataType,
                                    base::Unretained(polling_thread_),
                                    type));
}

void ProviderImpl::AddObserver(Observer* observer) {
  DCHECK(MessageLoop::current() == creator_loop_);

  DeviceData::Type type = observer->device_data_type();

  observers_.insert(observer);
  if (observers_.size() == 1)
    Start(type);
  else {
    // Notify observer of most recent notification if one exists.
    const DeviceData *last_notification = last_notifications_map_[type];
    if (last_notification != NULL)
      observer->OnDeviceDataUpdate(last_notification, type);
  }

  ScheduleDoAddPollingDataType(type);
}

void ProviderImpl::RemoveObserver(Observer* observer) {
  DCHECK(MessageLoop::current() == creator_loop_);

  observers_.erase(observer);
  if (observers_.empty())
    Stop();
}

void ProviderImpl::Start(DeviceData::Type type) {
  DCHECK(MessageLoop::current() == creator_loop_);
  DCHECK(!polling_thread_);

  polling_thread_ = new PollingThread("Device data polling thread",
                                      weak_factory_.GetWeakPtr(),
                                      creator_loop_);
  if (!polling_thread_->Start()) {
    LOG(ERROR) << "Failed to start device data polling thread";
    delete polling_thread_;
    polling_thread_ = NULL;
    return;
  }
  ScheduleInitializePollingThread(type);
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

void ProviderImpl::ScheduleInitializePollingThread(
    DeviceData::Type device_data_type) {
  DCHECK(MessageLoop::current() == creator_loop_);

  MessageLoop* polling_loop = polling_thread_->message_loop();
  polling_loop->PostTask(FROM_HERE,
                         base::Bind(&PollingThread::Initialize,
                                    base::Unretained(polling_thread_),
                                    factory_,
                                    device_data_type));
}

void ProviderImpl::DoNotify(const scoped_refptr<const DeviceData>& data,
    DeviceData::Type device_data_type) {
  DCHECK(MessageLoop::current() == creator_loop_);

  // Update last notification of this type.
  last_notifications_map_[device_data_type] = data;

  // Notify observers of this type of the new data.
  typedef std::set<Observer*>::const_iterator ConstIterator;
  for (ConstIterator i = observers_.begin(); i != observers_.end(); ++i) {
    if ((*i)->device_data_type() == device_data_type)
      (*i)->OnDeviceDataUpdate(data.get(), device_data_type);
  }

  if (data == NULL) {
    // Notify observers exactly once about failure to provide data.
    typedef std::set<Observer*>::iterator Iterator;
    Iterator i = observers_.begin();
    while (i != observers_.end()) {
      Iterator current = i++;
      if ((*current)->device_data_type() == device_data_type)
        observers_.erase(current);
    }

    if (observers_.empty())
      Stop();
  }
}


}  // namespace content
