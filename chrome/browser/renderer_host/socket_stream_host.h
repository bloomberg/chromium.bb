// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_SOCKET_STREAM_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_SOCKET_STREAM_HOST_H_

#include <vector>

#include "base/ref_counted.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "net/socket_stream/socket_stream.h"

class GURL;

// Host of SocketStreamHandle.
// Each SocketStreamHandle will have an unique socket_id assigned by
// SocketStreamHost constructor. If socket id is chrome_common_net::kNoSocketId,
// there is no SocketStreamHost.
// Each SocketStreamHost has SocketStream to manage bi-directional
// communication over socket stream.
// The lifetime of an instance of this class is completely controlled by the
// SocketStreamDispatcherHost.
class SocketStreamHost {
 public:
  SocketStreamHost(net::SocketStream::Delegate* delegate,
                   ResourceDispatcherHost::Receiver* receiver,
                   int socket_id);
  ~SocketStreamHost();

  // Gets SocketStreamHost associated with |socket|.
  static SocketStreamHost* GetSocketStreamHost(net::SocketStream* socket);

  ResourceDispatcherHost::Receiver* receiver() const { return receiver_; }
  int socket_id() const { return socket_id_; }

  // Starts to open connection to |url|.
  void Connect(const GURL& url);

  // Sends |data| over the socket stream.
  // socket stream must be open to send data.
  // Returns true if the data is put in transmit buffer in socket stream.
  // Returns false otherwise (transmit buffer exceeds limit, or socket
  // stream is closed).
  bool SendData(const std::vector<char>& data);

  // Closes the socket stream.
  void Close();

  bool Connected(int max_pending_send_allowed);

  bool SentData(int amount_sent);

  bool ReceivedData(const char* data, int len);

 private:
  net::SocketStream::Delegate* delegate_;
  ResourceDispatcherHost::Receiver* receiver_;
  int socket_id_;

  scoped_refptr<net::SocketStream> socket_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_SOCKET_STREAM_HOST_H_
