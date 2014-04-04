// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_ADB_WEB_SOCKET_H_
#define CHROME_BROWSER_DEVTOOLS_ADB_WEB_SOCKET_H_

#include "chrome/browser/devtools/android_device.h"

class AdbWebSocket : public base::RefCountedThreadSafe<AdbWebSocket> {
 public:
  class Delegate {
   public:
    virtual void OnSocketOpened() = 0;
    virtual void OnFrameRead(const std::string& message) = 0;
    virtual void OnSocketClosed(bool closed_by_device) = 0;

   protected:
    virtual ~Delegate() {}
  };

  AdbWebSocket(scoped_refptr<AndroidDevice> device,
               const std::string& socket_name,
               const std::string& url,
               base::MessageLoop* adb_message_loop,
               Delegate* delegate);

  void Disconnect();

  void SendFrame(const std::string& message);

 private:
  friend class base::RefCountedThreadSafe<AdbWebSocket>;

  virtual ~AdbWebSocket();

  void ConnectOnHandlerThread();
  void ConnectedOnHandlerThread(int result, net::StreamSocket* socket);
  void StartListeningOnHandlerThread();
  void OnBytesRead(scoped_refptr<net::IOBuffer> response_buffer, int result);
  void SendFrameOnHandlerThread(const std::string& message);
  void SendPendingRequests(int result);
  void DisconnectOnHandlerThread(bool closed_by_device);

  void OnSocketOpened();
  void OnFrameRead(const std::string& message);
  void OnSocketClosed(bool closed_by_device);

  scoped_refptr<AndroidDevice> device_;
  std::string socket_name_;
  std::string url_;
  base::MessageLoop* adb_message_loop_;
  scoped_ptr<net::StreamSocket> socket_;
  Delegate* delegate_;
  std::string response_buffer_;
  std::string request_buffer_;
  DISALLOW_COPY_AND_ASSIGN(AdbWebSocket);
};

#endif  // CHROME_BROWSER_DEVTOOLS_ADB_WEB_SOCKET_H_
