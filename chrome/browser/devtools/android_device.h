// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_ANDROID_DEVICE_H_
#define CHROME_BROWSER_DEVTOOLS_ANDROID_DEVICE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/devtools/adb/android_usb_device.h"
#include "chrome/browser/devtools/refcounted_adb_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "net/socket/stream_socket.h"

class AndroidDevice : public base::RefCounted<AndroidDevice> {
 public:
  typedef base::Callback<void(int, const std::string&)> CommandCallback;
  typedef base::Callback<void(int result, net::StreamSocket*)> SocketCallback;

  AndroidDevice(const std::string& serial, bool is_connected);

  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) = 0;
  virtual void OpenSocket(const std::string& socket_name,
                          const SocketCallback& callback) = 0;
  virtual void HttpQuery(const std::string& la_name,
                         const std::string& request,
                         const CommandCallback& callback);
  void HttpUpgrade(const std::string& la_name,
                   const std::string& request,
                   const SocketCallback& callback);

  std::string serial() { return serial_; }
  bool is_connected() { return is_connected_; }

  std::string model() { return model_; }
  void set_model(const std::string& model) { model_ = model; }

 protected:
  friend class base::RefCounted<AndroidDevice>;
  virtual ~AndroidDevice();

 private:
  void OnHttpSocketOpened(const std::string& request,
                          const CommandCallback& callback,
                          int result,
                          net::StreamSocket* socket);
  void OnHttpSocketOpened2(const std::string& request,
                           const SocketCallback& callback,
                           int result,
                           net::StreamSocket* socket);

  std::string serial_;
  bool is_connected_;
  std::string model_;

  DISALLOW_COPY_AND_ASSIGN(AndroidDevice);
};


class AndroidDeviceProvider
    : public base::RefCountedThreadSafe<
          AndroidDeviceProvider,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  typedef std::vector<scoped_refptr<AndroidDevice> > AndroidDevices;
  typedef base::Callback<void(const AndroidDevices&)> QueryDevicesCallback;

  virtual void QueryDevices(const QueryDevicesCallback& callback) = 0;

  static void CountDevices(bool discover_usb_devices,
                           const  base::Callback<void(int)>& callback);

  static scoped_refptr<AndroidDeviceProvider> GetAdbDeviceProvider();
  static scoped_refptr<AndroidDeviceProvider>
      GetUsbDeviceProvider(Profile* profile);

 protected:
  friend struct
      content::BrowserThread::DeleteOnThread<content::BrowserThread::UI>;
  friend class base::DeleteHelper<AndroidDeviceProvider>;

  AndroidDeviceProvider();
  virtual ~AndroidDeviceProvider();
  static void RunCallbackOnUIThread(const QueryDevicesCallback& callback,
                                    const AndroidDevices& result);

  scoped_refptr<RefCountedAdbThread> adb_thread_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_ANDROID_DEVICE_H_
