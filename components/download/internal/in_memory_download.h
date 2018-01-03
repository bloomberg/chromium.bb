// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_IN_MEMORY_DOWNLOAD_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_IN_MEMORY_DOWNLOAD_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/download/internal/blob_task_proxy.h"
#include "components/download/public/download_params.h"
#include "net/base/completion_callback.h"
#include "net/url_request/url_fetcher_delegate.h"
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

// Object to start a single download and hold in-memory download data.
// Used by download service in Incognito mode, where download files shouldn't
// be persisted to disk.
//
// Life cycle: The object is created before creating the network request.
// Call Start() to send the network request.
//
// Threading contract:
// 1. This object lives on the main thread.
// 2. Reading/writing IO buffer from network is done on another thread,
// based on |request_context_getter_|. When complete, main thread is notified.
// 3. After network IO is done, Blob related work is done on IO thread with
// |blob_task_proxy_|, then notify the result to main thread.

class InMemoryDownload : public net::URLFetcherDelegate {
 public:
  // Report download progress with in-memory download backend.
  class Delegate {
   public:
    virtual void OnDownloadProgress(InMemoryDownload* download) = 0;
    virtual void OnDownloadComplete(InMemoryDownload* download) = 0;

   protected:
    virtual ~Delegate() = default;
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

  InMemoryDownload(
      const std::string& guid,
      const RequestParams& request_params,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      Delegate* delegate,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      BlobTaskProxy::BlobContextGetter blob_context_getter,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~InMemoryDownload() override;

  // Send the download request.
  void Start();

  // Get a copy of blob data handle.
  std::unique_ptr<storage::BlobDataHandle> ResultAsBlob();

  const std::string& guid() const { return guid_; }
  uint64_t bytes_downloaded() const { return bytes_downloaded_; }
  State state() const { return state_; }

 private:
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

  // Notifies the delegate about completion.
  void NotifyDelegateDownloadComplete();

  // GUID of the download.
  const std::string guid_;

  // Request parameters of the download.
  const RequestParams request_params_;

  // Traffic annotation of the request.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // Used to send requests to servers. Also contains the download data in its
  // string buffer. We should avoid extra copy on the data and release the
  // memory when needed.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // Request context getter used by |url_fetcher_|.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // Worker that does blob related task on IO thread.
  std::unique_ptr<BlobTaskProxy> blob_task_proxy_;

  // Owned blob data handle, so that blob system keeps at least one reference
  // count of the underlying data.
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle_;

  // Used to access blob storage context.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  State state_;

  Delegate* delegate_;

  uint64_t bytes_downloaded_;

  // Bounded to main thread task runner.
  base::WeakPtrFactory<InMemoryDownload> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryDownload);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_IN_MEMORY_DOWNLOAD_H_
