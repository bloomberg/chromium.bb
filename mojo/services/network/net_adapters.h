// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NET_ADAPTERS_H_
#define MOJO_SERVICES_NETWORK_NET_ADAPTERS_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/services/network/public/interfaces/network_error.mojom.h"
#include "net/base/io_buffer.h"

namespace mojo {

// These adapters are used to transfer data between a Mojo pipe and the net
// library. There are four adapters, one for each end in each direction:
//
//   Mojo pipe              Data flow    Network library
//   ----------------------------------------------------------
//   MojoToNetPendingBuffer    --->      MojoToNetIOBuffer
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
  static MojoResult BeginWrite(ScopedDataPipeProducerHandle* handle,
                               scoped_refptr<NetToMojoPendingBuffer>* pending,
                               uint32_t* num_bytes);

  // Called to indicate the buffer is done being written to. Passes ownership
  // of the pipe back to the caller.
  ScopedDataPipeProducerHandle Complete(uint32_t num_bytes);

  char* buffer() { return static_cast<char*>(buffer_); }

 private:
  friend class base::RefCountedThreadSafe<NetToMojoPendingBuffer>;

  // Takes ownership of the handle.
  NetToMojoPendingBuffer(ScopedDataPipeProducerHandle handle, void* buffer);
  ~NetToMojoPendingBuffer();

  ScopedDataPipeProducerHandle handle_;
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

// Mojo side of a Mojo -> Net copy.
class MojoToNetPendingBuffer
    : public base::RefCountedThreadSafe<MojoToNetPendingBuffer> {
 public:
  // Starts reading from Mojo.
  //
  // On success, MOJO_RESULT_OK will be returned. The ownership of the given
  // consumer handle will be transferred to the new MojoToNetPendingBuffer that
  // will be placed into *pending, and the size of the buffer will be in
  // *num_bytes.
  //
  // On failure or MOJO_RESULT_SHOULD_WAIT, there will be no change to the
  // handle, and *pending and *num_bytes will be unused.
  static MojoResult BeginRead(ScopedDataPipeConsumerHandle* handle,
                              scoped_refptr<MojoToNetPendingBuffer>* pending,
                              uint32_t* num_bytes);

  // Indicates the buffer is done being read from. Passes ownership of the pipe
  // back to the caller. The argument is the number of bytes actually read,
  // since net may do partial writes, which will result in partial reads from
  // the Mojo pipe's perspective.
  ScopedDataPipeConsumerHandle Complete(uint32_t num_bytes);

  const char* buffer() { return static_cast<const char*>(buffer_); }

 private:
  friend class base::RefCountedThreadSafe<MojoToNetPendingBuffer>;

  // Takes ownership of the handle.
  explicit MojoToNetPendingBuffer(ScopedDataPipeConsumerHandle handle,
                                  const void* buffer);
  ~MojoToNetPendingBuffer();

  ScopedDataPipeConsumerHandle handle_;
  const void* buffer_;

  DISALLOW_COPY_AND_ASSIGN(MojoToNetPendingBuffer);
};

// Net side of a Mojo -> Net copy. The data will already be in the
// MojoToNetPendingBuffer's buffer.
class MojoToNetIOBuffer : public net::WrappedIOBuffer {
 public:
  explicit MojoToNetIOBuffer(MojoToNetPendingBuffer* pending_buffer);

 private:
  ~MojoToNetIOBuffer() override;

  scoped_refptr<MojoToNetPendingBuffer> pending_buffer_;
};

// Creates a new Mojo network error object from a net error code.
NetworkErrorPtr MakeNetworkError(int error_code);

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_NET_ADAPTERS_H_
