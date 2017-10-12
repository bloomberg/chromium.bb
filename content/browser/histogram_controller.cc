// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/histogram_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "content/browser/histogram_subscriber.h"
#include "content/common/child_process_messages.h"
#include "content/common/histogram_fetcher.mojom.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/process_type.h"

namespace content {

HistogramController* HistogramController::GetInstance() {
  return base::Singleton<HistogramController, base::LeakySingletonTraits<
                                                  HistogramController>>::get();
}

HistogramController::HistogramController() : subscriber_(NULL) {
}

HistogramController::~HistogramController() {
}

void HistogramController::OnPendingProcesses(int sequence_number,
                                             int pending_processes,
                                             bool end) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (subscriber_)
    subscriber_->OnPendingProcesses(sequence_number, pending_processes, end);
}

void HistogramController::OnHistogramDataCollected(
    int sequence_number,
    const std::vector<std::string>& pickled_histograms) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&HistogramController::OnHistogramDataCollected,
                       base::Unretained(this), sequence_number,
                       pickled_histograms));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (subscriber_) {
    subscriber_->OnHistogramDataCollected(sequence_number,
                                          pickled_histograms);
  }
}

class FetcherCallbackRunner {
 public:
  FetcherCallbackRunner(int sequence_number)
      : sequence_number_(sequence_number), did_run_(false) {}

  ~FetcherCallbackRunner() {
    if (!did_run_) {
      Run(std::vector<std::string>());
    }
  }
  static content::mojom::ChildHistogramFetcher::
      GetChildNonPersistentHistogramDataCallback
      Make(int sequence_number) {
    return base::BindOnce(
        &FetcherCallbackRunner::Run,
        std::make_unique<FetcherCallbackRunner>(sequence_number));
  }

  void Run(const std::vector<std::string>& pickled_histograms) {
    did_run_ = true;
    HistogramController::GetInstance()->OnHistogramDataCollected(
        sequence_number_, pickled_histograms);
  }

 private:
  int sequence_number_;
  bool did_run_;
};

void HistogramController::Register(HistogramSubscriber* subscriber) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!subscriber_);
  subscriber_ = subscriber;
}

void HistogramController::Unregister(
    const HistogramSubscriber* subscriber) {
  DCHECK_EQ(subscriber_, subscriber);
  subscriber_ = NULL;
}

template <class T>
void HistogramController::NotifyChildDied(T* host) {
  RemoveChildHistogramFetcherInterface(host);
}

template void HistogramController::NotifyChildDied(RenderProcessHost* host);

template <>
HistogramController::ChildHistogramFetcherMap<ChildProcessHost>&
HistogramController::GetChildHistogramFetcherMap() {
  return child_histogram_fetchers_;
}

template <>
HistogramController::ChildHistogramFetcherMap<RenderProcessHost>&
HistogramController::GetChildHistogramFetcherMap() {
  return renderer_histogram_fetchers_;
}

template void HistogramController::SetHistogramMemory(
    ChildProcessHost* host,
    mojo::ScopedSharedBufferHandle shared_buffer);

template void HistogramController::SetHistogramMemory(
    RenderProcessHost* host,
    mojo::ScopedSharedBufferHandle shared_buffer);

template <class T>
void HistogramController::SetHistogramMemory(
    T* host,
    mojo::ScopedSharedBufferHandle shared_buffer) {
  content::mojom::ChildHistogramFetcherFactoryPtr
      child_histogram_fetcher_factory;
  content::mojom::ChildHistogramFetcherPtr child_histogram_fetcher;
  content::BindInterface(host, &child_histogram_fetcher_factory);
  child_histogram_fetcher_factory->CreateFetcher(
      std::move(shared_buffer), mojo::MakeRequest(&child_histogram_fetcher));
  InsertChildHistogramFetcherInterface(host,
                                       std::move(child_histogram_fetcher));
}

template <class T>
void HistogramController::InsertChildHistogramFetcherInterface(
    T* host,
    content::mojom::ChildHistogramFetcherPtr child_histogram_fetcher) {
  // Broken pipe means remove this from the map. The map size is a proxy for
  // the number of known processes
  child_histogram_fetcher.set_connection_error_handler(
      base::Bind(&HistogramController::RemoveChildHistogramFetcherInterface<T>,
                 base::Unretained(this), base::Unretained(host)));
  GetChildHistogramFetcherMap<T>()[host] = std::move(child_histogram_fetcher);
}

template <class T>
content::mojom::ChildHistogramFetcher*
HistogramController::GetChildHistogramFetcherInterface(T* host) {
  auto it = GetChildHistogramFetcherMap<T>().find(host);
  if (it != GetChildHistogramFetcherMap<T>().end()) {
    return (it->second).get();
  }
  return nullptr;
}

template <class T>
void HistogramController::RemoveChildHistogramFetcherInterface(T* host) {
  GetChildHistogramFetcherMap<T>().erase(host);
}

void HistogramController::GetHistogramDataFromChildProcesses(
    int sequence_number) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  int pending_processes = 0;
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    const ChildProcessData& data = iter.GetData();

    // Only get histograms from content process types; skip "embedder" process
    // types.
    if (data.process_type >= PROCESS_TYPE_CONTENT_END)
      continue;

    // In some cases, there may be no child process of the given type (for
    // example, the GPU process may not exist and there may instead just be a
    // GPU thread in the browser process). If that's the case, then the process
    // handle will be base::kNullProcessHandle and we shouldn't ask it for data.
    if (data.handle == base::kNullProcessHandle)
      continue;

    if (auto* child_histogram_fetcher =
            GetChildHistogramFetcherInterface(iter.GetHost())) {
      child_histogram_fetcher->GetChildNonPersistentHistogramData(
          FetcherCallbackRunner::Make(sequence_number));
      ++pending_processes;
    }
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&HistogramController::OnPendingProcesses,
                     base::Unretained(this), sequence_number, pending_processes,
                     true));
}

void HistogramController::GetHistogramData(int sequence_number) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int pending_processes = 0;
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd() && it.GetCurrentValue()->IsReady(); it.Advance()) {
    if (auto* child_histogram_fetcher =
            GetChildHistogramFetcherInterface(it.GetCurrentValue())) {
      child_histogram_fetcher->GetChildNonPersistentHistogramData(
          FetcherCallbackRunner::Make(sequence_number));
      ++pending_processes;
    }
  }
  OnPendingProcesses(sequence_number, pending_processes, false);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&HistogramController::GetHistogramDataFromChildProcesses,
                     base::Unretained(this), sequence_number));
}

}  // namespace content
