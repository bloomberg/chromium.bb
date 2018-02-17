// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IN_MEMORY_DOWNLOAD_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IN_MEMORY_DOWNLOAD_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/download/internal/background_service/blob_task_proxy.h"
#include "components/download/public/background_service/download_params.h"
#include "net/base/completion_callback.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "net/url_request/url_request_context_getter.h"

namespace net {
class URLFetcher;
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace storage {
class BlobDataHandle;
}  // namespace storage

namespace download {

struct RequestParams;

// Class to start a single download and hold in-memory download data.
// Used by download service in Incognito mode, where download files shouldn't
// be persisted to disk.
//
// Life cycle: The object is created before creating the network request.
// Call Start() to send the network request.
class InMemoryDownload {
 public:
  // Report download progress with in-memory download backend.
  class Delegate {
   public:
    virtual void OnDownloadProgress(InMemoryDownload* download) = 0;
    virtual void OnDownloadComplete(InMemoryDownload* download) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Factory to create in memory download.
  class Factory {
   public:
    virtual std::unique_ptr<InMemoryDownload> Create(
        const std::string& guid,
        const RequestParams& request_params,
        const net::NetworkTrafficAnnotationTag& traffic_annotation,
        Delegate* delegate) = 0;

    virtual ~Factory() = default;
  };

  // States of the download.
  enum class State {
    // The object is created but network request has not been sent.
    INITIAL,

    // Download is in progress, including the following procedures.
    // 1. Transfer network data.
    // 2. Save to blob storage.
    IN_PROGRESS,

    // The download can fail due to:
    // 1. network layer failure or unsuccessful HTTP server response code.
    // 2. Blob system failures after blob construction is done.
    FAILED,

    // Download is completed, and data is successfully saved as a blob.
    // 1. We guarantee the states of network responses.
    // 2. Do not guarantee the state of blob data. The consumer of blob
    // should validate its state when using it on IO thread.
    COMPLETE,
  };

  virtual ~InMemoryDownload();

  // Send the download request.
  virtual void Start() = 0;

  // Pause the download request.
  virtual void Pause() = 0;

  // Resume the download request.
  virtual void Resume() = 0;

  // Get a copy of blob data handle.
  virtual std::unique_ptr<storage::BlobDataHandle> ResultAsBlob() = 0;

  // Returns the estimate of dynamically allocated memory in bytes.
  virtual size_t EstimateMemoryUsage() const = 0;

  const std::string& guid() const { return guid_; }
  uint64_t bytes_downloaded() const { return bytes_downloaded_; }
  State state() const { return state_; }
  const base::Time& completion_time() const { return completion_time_; }
  scoped_refptr<const net::HttpResponseHeaders> response_headers() const {
    return response_headers_;
  }

 protected:
  InMemoryDownload(const std::string& guid);

  // GUID of the download.
  const std::string guid_;

  State state_;

  // Completion time of download when data is saved as blob.
  base::Time completion_time_;

  // HTTP response headers.
  scoped_refptr<const net::HttpResponseHeaders> response_headers_;

  uint64_t bytes_downloaded_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InMemoryDownload);
};

// Implementation of InMemoryDownload and uses URLFetcher as network backend.
// Threading contract:
// 1. This object lives on the main thread.
// 2. Reading/writing IO buffer from network is done on another thread,
// based on |request_context_getter_|. When complete, main thread is notified.
// 3. After network IO is done, Blob related work is done on IO thread with
// |blob_task_proxy_|, then notify the result to main thread.
class InMemoryDownloadImpl : public net::URLFetcherDelegate,
                             public InMemoryDownload {
 public:
  InMemoryDownloadImpl(
      const std::string& guid,
      const RequestParams& request_params,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Delegate* delegate,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      BlobTaskProxy::BlobContextGetter blob_context_getter,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~InMemoryDownloadImpl() override;

 private:
  // Response writer that supports pause and resume operations.
  class ResponseWriter : public net::URLFetcherResponseWriter {
   public:
    ResponseWriter(scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
    ~ResponseWriter() override;

    // Pause writing data from pipe into |data_|.
    void Pause();

    // Resume writing data from the pipe into |data_|.
    void Resume();

    // Take the data, must be called after the network layer completes its job.
    std::unique_ptr<std::string> TakeData();

   private:
    // net::URLFetcherResponseWriter implementation.
    int Initialize(const net::CompletionCallback& callback) override;
    int Write(net::IOBuffer* buffer,
              int num_bytes,
              const net::CompletionCallback& callback) override;
    int Finish(int net_error, const net::CompletionCallback& callback) override;

    void PauseOnIO();
    void ResumeOnIO();

    // Download data, should be moved to avoid extra copy.
    std::unique_ptr<std::string> data_;

    bool paused_on_io_;
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

    // When paused, cached callback to trigger the next read. Must be set and
    // called on fetcher's IO thread.
    net::CompletionCallback write_callback_;

    DISALLOW_COPY_AND_ASSIGN(ResponseWriter);
  };

  // InMemoryDownload implementation.
  void Start() override;
  void Pause() override;
  void Resume() override;

  std::unique_ptr<storage::BlobDataHandle> ResultAsBlob() override;
  size_t EstimateMemoryUsage() const override;

  // net::URLFetcherDelegate implementation.
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64_t current,
                                  int64_t total,
                                  int64_t current_network_bytes) override;
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Handles response code and change the state accordingly.
  // Returns if the response code is considered as successful code.
  bool HandleResponseCode(int response_code);

  // Saves the download data into blob storage.
  void SaveAsBlob();
  void OnSaveBlobDone(std::unique_ptr<storage::BlobDataHandle> blob_handle,
                      storage::BlobStatus status);

  // Notifies the delegate about completion. Can be called multiple times and
  // |completion_notified_| will ensure the delegate only receive one completion
  // call.
  void NotifyDelegateDownloadComplete();

  // Sends the network request.
  void SendRequest();

  // Request parameters of the download.
  const RequestParams request_params_;

  // Traffic annotation of the request.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // Used to send requests to servers. Also contains the download data in its
  // string buffer. We should avoid extra copy on the data and release the
  // memory when needed.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // Owned by |url_fetcher_|. Lives on fetcher's delegate thread, perform
  // network IO on fetcher's IO thread.
  ResponseWriter* response_writer_;

  // Request context getter used by |url_fetcher_|.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // Worker that does blob related task on IO thread.
  std::unique_ptr<BlobTaskProxy> blob_task_proxy_;

  // Owned blob data handle, so that blob system keeps at least one reference
  // count of the underlying data.
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle_;

  // Used to access blob storage context.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  Delegate* delegate_;

  bool paused_;

  // Ensures Delegate::OnDownloadComplete is only called once.
  bool completion_notified_;

  // Bounded to main thread task runner.
  base::WeakPtrFactory<InMemoryDownloadImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryDownloadImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_IN_MEMORY_DOWNLOAD_H_
