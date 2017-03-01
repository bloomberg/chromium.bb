// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_URL_LOADER_CLIENT_IMPL_H_
#define CONTENT_CHILD_URL_LOADER_CLIENT_IMPL_H_

#include <stdint.h>
#include <vector>
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/url_loader.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
struct RedirectInfo;
}  // namespace net

namespace content {
class ResourceDispatcher;
class URLResponseBodyConsumer;
struct ResourceRequestCompletionStatus;
struct ResourceResponseHead;

class CONTENT_EXPORT URLLoaderClientImpl final : public mojom::URLLoaderClient {
 public:
  URLLoaderClientImpl(int request_id,
                      ResourceDispatcher* resource_dispatcher,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~URLLoaderClientImpl() override;

  void Bind(mojom::URLLoaderClientPtr* client_ptr);

  // Sets |is_deferred_|. From now, the received messages are not dispatched
  // to clients until UnsetDefersLoading is called.
  void SetDefersLoading();

  // Unsets |is_deferred_|.
  void UnsetDefersLoading();

  // Disaptches the messages received after SetDefersLoading is called.
  void FlushDeferredMessages();

  // mojom::URLLoaderClient implementation
  void OnReceiveResponse(const ResourceResponseHead& response_head,
                         mojom::DownloadedTempFilePtr downloaded_file) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override;
  void OnDataDownloaded(int64_t data_len, int64_t encoded_data_len) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        const base::Closure& ack_callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const ResourceRequestCompletionStatus& status) override;

 private:
  void Dispatch(const IPC::Message& message);

  mojo::Binding<mojom::URLLoaderClient> binding_;
  scoped_refptr<URLResponseBodyConsumer> body_consumer_;
  mojom::DownloadedTempFilePtr downloaded_file_;
  std::vector<IPC::Message> deferred_messages_;
  const int request_id_;
  bool has_received_response_ = false;
  bool is_deferred_ = false;
  int32_t accumulated_transfer_size_diff_during_deferred_ = 0;
  ResourceDispatcher* const resource_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<URLLoaderClientImpl> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_CHILD_URL_LOADER_CLIENT_IMPL_H_
