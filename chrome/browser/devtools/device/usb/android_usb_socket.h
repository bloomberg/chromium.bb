// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_USB_SOCKET_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_USB_SOCKET_H_

#include <deque>

#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/devtools/device/usb/android_usb_device.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace base {
class MessageLoop;
}

class AdbMessage;

class AndroidUsbSocket : public net::StreamSocket,
                         public base::NonThreadSafe {
 public:
  AndroidUsbSocket(scoped_refptr<AndroidUsbDevice> device,
                   uint32 socket_id,
                   const std::string& command,
                   base::Callback<void(uint32)> delete_callback);
  virtual ~AndroidUsbSocket();

  void HandleIncoming(scoped_refptr<AdbMessage> message);

  void Terminated(bool closed_by_device);

  // net::StreamSocket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) override;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) override;
  virtual int SetReceiveBufferSize(int32 size) override;
  virtual int SetSendBufferSize(int32 size) override;
  virtual int Connect(const net::CompletionCallback& callback) override;
  virtual void Disconnect() override;
  virtual bool IsConnected() const override;
  virtual bool IsConnectedAndIdle() const override;
  virtual int GetPeerAddress(net::IPEndPoint* address) const override;
  virtual int GetLocalAddress(net::IPEndPoint* address) const override;
  virtual const net::BoundNetLog& NetLog() const override;
  virtual void SetSubresourceSpeculation() override;
  virtual void SetOmniboxSpeculation() override;
  virtual bool WasEverUsed() const override;
  virtual bool UsingTCPFastOpen() const override;
  virtual bool WasNpnNegotiated() const override;
  virtual net::NextProto GetNegotiatedProtocol() const override;
  virtual bool GetSSLInfo(net::SSLInfo* ssl_info) override;

 private:
  class IORequest {
   public:
    IORequest(net::IOBuffer* buffer,
              int length,
              const net::CompletionCallback& callback);
    ~IORequest();

    scoped_refptr<net::IOBuffer> buffer;
    int length;
    net::CompletionCallback callback;
  };

  void RespondToReaders(bool diconnect);
  void RespondToWriters();

  scoped_refptr<AndroidUsbDevice> device_;
  std::string command_;
  base::Callback<void(uint32)> delete_callback_;
  uint32 local_id_;
  uint32 remote_id_;
  net::BoundNetLog net_log_;
  bool is_connected_;
  std::string read_buffer_;
  net::CompletionCallback connect_callback_;
  std::deque<IORequest> read_requests_;
  std::deque<IORequest> write_requests_;

  DISALLOW_COPY_AND_ASSIGN(AndroidUsbSocket);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_USB_SOCKET_H_
