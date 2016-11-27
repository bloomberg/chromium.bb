// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_USB_SOCKET_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_USB_SOCKET_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/devtools/device/usb/android_usb_device.h"
#include "net/base/ip_endpoint.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket.h"

class AndroidUsbSocket : public net::StreamSocket,
                         public base::NonThreadSafe {
 public:
  AndroidUsbSocket(scoped_refptr<AndroidUsbDevice> device,
                   uint32_t socket_id,
                   const std::string& command,
                   base::Closure delete_callback);
  ~AndroidUsbSocket() override;

  void HandleIncoming(std::unique_ptr<AdbMessage> message);

  void Terminated(bool closed_by_device);

  // net::StreamSocket implementation.
  int Read(net::IOBuffer* buf,
           int buf_len,
           const net::CompletionCallback& callback) override;
  int Write(net::IOBuffer* buf,
            int buf_len,
            const net::CompletionCallback& callback) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int Connect(const net::CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  const net::NetLogWithSource& NetLog() const override;
  void SetSubresourceSpeculation() override;
  void SetOmniboxSpeculation() override;
  bool WasEverUsed() const override;
  bool WasNpnNegotiated() const override;
  net::NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(net::SSLInfo* ssl_info) override;
  void GetConnectionAttempts(net::ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const net::ConnectionAttempts& attempts) override {
  }
  int64_t GetTotalReceivedBytes() const override;

 private:
  void RespondToReader(bool disconnect);
  void RespondToWriter(int result);

  scoped_refptr<AndroidUsbDevice> device_;
  std::string command_;
  uint32_t local_id_;
  uint32_t remote_id_;
  net::NetLogWithSource net_log_;
  bool is_connected_;
  std::string read_buffer_;
  scoped_refptr<net::IOBuffer> read_io_buffer_;
  int read_length_;
  int write_length_;
  net::CompletionCallback connect_callback_;
  net::CompletionCallback read_callback_;
  net::CompletionCallback write_callback_;
  base::Closure delete_callback_;
  base::WeakPtrFactory<AndroidUsbSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidUsbSocket);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_USB_SOCKET_H_
