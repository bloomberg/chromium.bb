// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SOURCE_STREAM_TO_DATA_PIPE_H_
#define CONTENT_BROWSER_LOADER_SOURCE_STREAM_TO_DATA_PIPE_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/cpp/net_adapters.h"

namespace net {
class SourceStream;
}

namespace content {

// A convenient adapter class to read out data from net::SourceStream
// and write them into a data pipe.
class SourceStreamToDataPipe {
 public:
  // Reads out the data from |source| and write into |dest|.
  // Note that this does not take the ownership of |source|, the caller
  // needs to take care that it is kept alive.
  SourceStreamToDataPipe(net::SourceStream* source,
                         mojo::ScopedDataPipeProducerHandle dest,
                         base::OnceCallback<void(int)> completion_callback);
  ~SourceStreamToDataPipe();

  // Start reading the source.
  void Start();

 private:
  void ReadMore();
  void DidRead(int result);

  void OnDataPipeWritable(MojoResult result);
  void OnDataPipeClosed(MojoResult result);
  void OnComplete(int result);

  net::SourceStream* source_;  // Not owned.
  mojo::ScopedDataPipeProducerHandle dest_;
  base::OnceCallback<void(int)> completion_callback_;

  scoped_refptr<network::NetToMojoPendingBuffer> pending_write_;
  mojo::SimpleWatcher writable_handle_watcher_;
  mojo::SimpleWatcher peer_closed_handle_watcher_;

  base::WeakPtrFactory<SourceStreamToDataPipe> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SOURCE_STREAM_TO_DATA_PIPE_H_
