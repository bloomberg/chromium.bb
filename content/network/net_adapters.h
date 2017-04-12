// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NET_ADAPTERS_
#define CONTENT_NETWORK_NET_ADAPTERS_

#include <stdint.h>

#include "base/macros.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"

namespace content {

// These adapters are used to transfer data between a Mojo pipe and the net
// library.
//
//   Mojo pipe              Data flow    Network library
//   ----------------------------------------------------------
//   NetToMojoPendingBuffer    <---      NetToMojoIOBuffer
//
// While the operation is in progress, the Mojo-side objects keep ownership
// of the Mojo pipe, which in turn is kept alive by the IOBuffer. This allows
// the request to potentially outlive the object managing the translation.
// Mojo side of a Net -> Mojo copy. The buffer is allocated by Mojo.
class NetToMojoPendingBuffer
    : public base::RefCountedThreadSafe<NetToMojoPendingBuffer> {
 public:
  // Begins a two-phase write to the data pipe.
  //
  // On success, MOJO_RESULT_OK will be returned. The ownership of the given
  // producer handle will be transferred to the new NetToMojoPendingBuffer that
  // will be placed into *pending, and the size of the buffer will be in
  // *num_bytes.
  //
  // On failure or MOJO_RESULT_SHOULD_WAIT, there will be no change to the
  // handle, and *pending and *num_bytes will be unused.
  static MojoResult BeginWrite(mojo::ScopedDataPipeProducerHandle* handle,
                               scoped_refptr<NetToMojoPendingBuffer>* pending,
                               uint32_t* num_bytes);
  // Called to indicate the buffer is done being written to. Passes ownership
  // of the pipe back to the caller.
  mojo::ScopedDataPipeProducerHandle Complete(uint32_t num_bytes);
  char* buffer() { return static_cast<char*>(buffer_); }

 private:
  friend class base::RefCountedThreadSafe<NetToMojoPendingBuffer>;
  // Takes ownership of the handle.
  NetToMojoPendingBuffer(mojo::ScopedDataPipeProducerHandle handle,
                         void* buffer);
  ~NetToMojoPendingBuffer();
  mojo::ScopedDataPipeProducerHandle handle_;
  void* buffer_;
  DISALLOW_COPY_AND_ASSIGN(NetToMojoPendingBuffer);
};

// Net side of a Net -> Mojo copy. The data will be read from the network and
// copied into the buffer associated with the pending mojo write.
class NetToMojoIOBuffer : public net::WrappedIOBuffer {
 public:
  explicit NetToMojoIOBuffer(NetToMojoPendingBuffer* pending_buffer);

 private:
  ~NetToMojoIOBuffer() override;
  scoped_refptr<NetToMojoPendingBuffer> pending_buffer_;
};

}  // namespace content

#endif  // CONTENT_NETWORK_NET_ADAPTERS_
