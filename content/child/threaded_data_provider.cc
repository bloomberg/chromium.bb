// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/threaded_data_provider.h"

#include <stddef.h>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "components/scheduler/child/webthread_impl_for_worker_scheduler.h"
#include "content/child/child_process.h"
#include "content/child/child_thread_impl.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/resource_messages.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/WebKit/public/platform/WebThread.h"
#include "third_party/WebKit/public/platform/WebThreadedDataReceiver.h"

namespace content {

namespace {

class DataProviderMessageFilter : public IPC::MessageFilter {
 public:
  DataProviderMessageFilter(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      const scheduler::WebThreadImplForWorkerScheduler& background_thread,
      const base::WeakPtr<ThreadedDataProvider>&
          background_thread_resource_provider,
      const base::WeakPtr<ThreadedDataProvider>& main_thread_resource_provider,
      int request_id);

  // IPC::ChannelProxy::MessageFilter
  void OnFilterAdded(IPC::Sender* sender) final;
  bool OnMessageReceived(const IPC::Message& message) final;

 private:
  ~DataProviderMessageFilter() override {}

  void OnReceivedData(int request_id, int data_offset, int data_length,
                      int encoded_data_length);

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  const scheduler::WebThreadImplForWorkerScheduler& background_thread_;
  // This weakptr can only be dereferenced on the background thread.
  base::WeakPtr<ThreadedDataProvider>
      background_thread_resource_provider_;
  // This weakptr can only be dereferenced on the main thread.
  base::WeakPtr<ThreadedDataProvider>
      main_thread_resource_provider_;
  int request_id_;
};

DataProviderMessageFilter::DataProviderMessageFilter(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    const scheduler::WebThreadImplForWorkerScheduler& background_thread,
    const base::WeakPtr<ThreadedDataProvider>&
        background_thread_resource_provider,
    const base::WeakPtr<ThreadedDataProvider>& main_thread_resource_provider,
    int request_id)
    : io_task_runner_(io_task_runner),
      main_thread_task_runner_(main_thread_task_runner),
      background_thread_(background_thread),
      background_thread_resource_provider_(background_thread_resource_provider),
      main_thread_resource_provider_(main_thread_resource_provider),
      request_id_(request_id) {
  DCHECK(main_thread_task_runner_.get());
}

void DataProviderMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ThreadedDataProvider::OnResourceMessageFilterAddedMainThread,
                 main_thread_resource_provider_));
}

bool DataProviderMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (message.type() != ResourceMsg_DataReceived::ID)
    return false;

  int request_id;

  base::PickleIterator iter(message);
  if (!iter.ReadInt(&request_id)) {
    NOTREACHED() << "malformed resource message";
    return true;
  }

  if (request_id == request_id_) {
    ResourceMsg_DataReceived::Schema::Param arg;
    if (ResourceMsg_DataReceived::Read(&message, &arg)) {
      OnReceivedData(base::get<0>(arg), base::get<1>(arg),
                     base::get<2>(arg), base::get<3>(arg));
      return true;
    }
  }

  return false;
}

void DataProviderMessageFilter::OnReceivedData(int request_id,
                                               int data_offset,
                                               int data_length,
                                               int encoded_data_length) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  background_thread_.TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadedDataProvider::OnReceivedDataOnBackgroundThread,
                 background_thread_resource_provider_, data_offset, data_length,
                 encoded_data_length));
}

}  // anonymous namespace

ThreadedDataProvider::ThreadedDataProvider(
    int request_id,
    blink::WebThreadedDataReceiver* threaded_data_receiver,
    linked_ptr<base::SharedMemory> shm_buffer,
    int shm_size,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : request_id_(request_id),
      shm_buffer_(shm_buffer),
      shm_size_(shm_size),
      background_thread_(
          static_cast<scheduler::WebThreadImplForWorkerScheduler&>(
              *threaded_data_receiver->backgroundThread())),
      ipc_channel_(ChildThreadImpl::current()->channel()),
      threaded_data_receiver_(threaded_data_receiver),
      resource_filter_active_(false),
      main_thread_task_runner_(main_thread_task_runner),
      main_thread_weak_factory_(this) {
  DCHECK(ChildThreadImpl::current());
  DCHECK(ipc_channel_);
  DCHECK(threaded_data_receiver_);
  DCHECK(main_thread_task_runner_.get());

  background_thread_weak_factory_.reset(
      new base::WeakPtrFactory<ThreadedDataProvider>(this));

  filter_ = new DataProviderMessageFilter(
      ChildProcess::current()->io_task_runner(), main_thread_task_runner_,
      background_thread_, background_thread_weak_factory_->GetWeakPtr(),
      main_thread_weak_factory_.GetWeakPtr(), request_id);

  ChildThreadImpl::current()->channel()->AddFilter(filter_.get());
}

ThreadedDataProvider::~ThreadedDataProvider() {
  DCHECK(ChildThreadImpl::current());

  ChildThreadImpl::current()->channel()->RemoveFilter(filter_.get());

  delete threaded_data_receiver_;
}

void ThreadedDataProvider::DestructOnMainThread() {
  DCHECK(ChildThreadImpl::current());

  // The ThreadedDataProvider must be destructed on the main thread to
  // be threadsafe when removing the message filter and releasing the shared
  // memory buffer.
  delete this;
}

void ThreadedDataProvider::Stop() {
  DCHECK(ChildThreadImpl::current());

  // Make sure we don't get called by on the main thread anymore via weak
  // pointers we've passed to the filter.
  main_thread_weak_factory_.InvalidateWeakPtrs();

  blink::WebThread* current_background_thread =
      threaded_data_receiver_->backgroundThread();

  // We can't destroy this instance directly; we need to bounce a message over
  // to the background thread and back to make sure nothing else will access it
  // there, before we can destruct it. We also need to make sure the background
  // thread is still alive, since Blink could have shut down at this point
  // and freed the thread.
  if (current_background_thread) {
    // We should never end up with a different parser thread than from when the
    // ThreadedDataProvider gets created.
    DCHECK(current_background_thread ==
           static_cast<scheduler::WebThreadImplForWorkerScheduler*>(
               &background_thread_));
    background_thread_.TaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ThreadedDataProvider::StopOnBackgroundThread,
                              base::Unretained(this)));
  }
}

void ThreadedDataProvider::StopOnBackgroundThread() {
  DCHECK(background_thread_.isCurrentThread());
  DCHECK(background_thread_weak_factory_);

  // When this happens, the provider should no longer be called on the
  // background thread as it's about to be destroyed on the main thread.
  // Destructing the weak pointer factory means invalidating the weak pointers
  // which means no callbacks from the filter will happen and nothing else will
  // use this instance on the background thread.
  background_thread_weak_factory_.reset(NULL);
  main_thread_task_runner_->PostTask(FROM_HERE,
      base::Bind(&ThreadedDataProvider::DestructOnMainThread,
                 base::Unretained(this)));
}

void ThreadedDataProvider::OnRequestCompleteForegroundThread(
      base::WeakPtr<ResourceDispatcher> resource_dispatcher,
      const ResourceMsg_RequestCompleteData& request_complete_data,
      const base::TimeTicks& renderer_completion_time) {
  DCHECK(ChildThreadImpl::current());

  background_thread_.TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&ThreadedDataProvider::OnRequestCompleteBackgroundThread,
                 base::Unretained(this), resource_dispatcher,
                 request_complete_data, renderer_completion_time));
}

void ThreadedDataProvider::OnRequestCompleteBackgroundThread(
      base::WeakPtr<ResourceDispatcher> resource_dispatcher,
      const ResourceMsg_RequestCompleteData& request_complete_data,
      const base::TimeTicks& renderer_completion_time) {
  DCHECK(background_thread_.isCurrentThread());

  main_thread_task_runner_->PostTask(FROM_HERE,
      base::Bind(
          &ResourceDispatcher::CompletedRequestAfterBackgroundThreadFlush,
          resource_dispatcher,
          request_id_,
          request_complete_data,
          renderer_completion_time));
}

void ThreadedDataProvider::OnResourceMessageFilterAddedMainThread() {
  DCHECK(ChildThreadImpl::current());
  DCHECK(background_thread_weak_factory_);

  // We bounce this message from the I/O thread via the main thread and then
  // to our background thread, following the same path as incoming data before
  // our filter gets added, to make sure there's nothing still incoming.
  background_thread_.TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &ThreadedDataProvider::OnResourceMessageFilterAddedBackgroundThread,
          background_thread_weak_factory_->GetWeakPtr()));
}

void ThreadedDataProvider::OnResourceMessageFilterAddedBackgroundThread() {
  DCHECK(background_thread_.isCurrentThread());
  resource_filter_active_ = true;

  // At this point we know no more data is going to arrive from the main thread,
  // so we can process any data we've received directly from the I/O thread
  // in the meantime.
  if (!queued_data_.empty()) {
    std::vector<QueuedSharedMemoryData>::iterator iter = queued_data_.begin();
    for (; iter != queued_data_.end(); ++iter) {
      ForwardAndACKData(iter->data, iter->length, iter->encoded_length);
    }

    queued_data_.clear();
  }
}

void ThreadedDataProvider::OnReceivedDataOnBackgroundThread(
    int data_offset, int data_length, int encoded_data_length) {
  DCHECK(background_thread_.isCurrentThread());
  DCHECK(shm_buffer_ != NULL);

  CHECK_GE(shm_size_, data_offset + data_length);
  const char* data_ptr = static_cast<char*>(shm_buffer_->memory());
  CHECK(data_ptr);
  CHECK(data_ptr + data_offset);

  if (resource_filter_active_) {
    ForwardAndACKData(data_ptr + data_offset, data_length, encoded_data_length);
  } else {
    // There's a brief interval between the point where we know the filter
    // has been installed on the I/O thread, and when we know for sure there's
    // no more data coming in from the main thread (from before the filter
    // got added). If we get any data during that interval, we need to queue
    // it until we're certain we've processed all the main thread data to make
    // sure we forward (and ACK) everything in the right order.
    QueuedSharedMemoryData queued_data;
    queued_data.data = data_ptr + data_offset;
    queued_data.length = data_length;
    queued_data.encoded_length = encoded_data_length;
    queued_data_.push_back(queued_data);
  }
}

void ThreadedDataProvider::OnReceivedDataOnForegroundThread(
    const char* data, int data_length, int encoded_data_length) {
  DCHECK(ChildThreadImpl::current());

  background_thread_.TaskRunner()->PostTask(
      FROM_HERE, base::Bind(&ThreadedDataProvider::ForwardAndACKData,
                            base::Unretained(this), data, data_length,
                            encoded_data_length));
}

void ThreadedDataProvider::ForwardAndACKData(const char* data,
                                             int data_length,
                                             int encoded_data_length) {
  DCHECK(background_thread_.isCurrentThread());

  threaded_data_receiver_->acceptData(data, data_length);

  scoped_ptr<std::vector<char>> data_copy;
  if (threaded_data_receiver_->needsMainthreadDataCopy()) {
    data_copy.reset(new std::vector<char>(data, data + data_length));
  }

  main_thread_task_runner_->PostTask(FROM_HERE,
      base::Bind(&ThreadedDataProvider::DataNotifyForegroundThread,
          base::Unretained(this),
          base::Passed(&data_copy),
          data_length,
          encoded_data_length));

  ipc_channel_->Send(new ResourceHostMsg_DataReceived_ACK(request_id_));
}

void ThreadedDataProvider::DataNotifyForegroundThread(
    scoped_ptr<std::vector<char> > data_copy,
    int data_length,
    int encoded_data_length) {
  if (data_copy) {
    DCHECK(threaded_data_receiver_->needsMainthreadDataCopy());
    DCHECK_EQ((size_t)data_length, data_copy->size());
  }

  threaded_data_receiver_->acceptMainthreadDataNotification(
      (data_copy && !data_copy->empty()) ? &data_copy->front() : NULL,
      data_length, encoded_data_length);
}

}  // namespace content
