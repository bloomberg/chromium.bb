// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_ANDROID_DEVICE_MANAGER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_ANDROID_DEVICE_MANAGER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/size.h"

namespace net {
class StreamSocket;
}

class AndroidDeviceManager
    : public base::RefCountedThreadSafe<AndroidDeviceManager>,
      public base::NonThreadSafe {
 public:
  typedef base::Callback<void(int, const std::string&)> CommandCallback;
  typedef base::Callback<void(int result, net::StreamSocket*)> SocketCallback;

  struct BrowserInfo {
    BrowserInfo();

    enum Type {
      kTypeChrome,
      kTypeWebView,
      kTypeOther
    };

    std::string socket_name;
    std::string display_name;
    Type type;
  };

  struct DeviceInfo {
    DeviceInfo();
    ~DeviceInfo();

    std::string model;
    gfx::Size screen_size;
    std::vector<BrowserInfo> browser_info;
  };

  typedef base::Callback<void(const DeviceInfo&)> DeviceInfoCallback;

  class Device : public base::RefCounted<Device>,
                 public base::NonThreadSafe {
   protected:
    friend class AndroidDeviceManager;

    typedef AndroidDeviceManager::CommandCallback CommandCallback;
    typedef AndroidDeviceManager::SocketCallback SocketCallback;
    typedef AndroidDeviceManager::DeviceInfoCallback DeviceInfoCallback;

    Device(const std::string& serial, bool is_connected);

    virtual void QueryDeviceInfo(const DeviceInfoCallback& callback) = 0;

    virtual void OpenSocket(const std::string& socket_name,
                            const SocketCallback& callback) = 0;

    virtual void HttpQuery(const std::string& socket_name,
                           const std::string& request,
                           const CommandCallback& callback);

    std::string serial() { return serial_; }
    bool is_connected() { return is_connected_; }

    friend class base::RefCounted<Device>;
    virtual ~Device();

   private:
    const std::string serial_;
    const bool is_connected_;

    DISALLOW_COPY_AND_ASSIGN(Device);
  };

  typedef std::vector<scoped_refptr<Device> > Devices;

  class DeviceProvider
      : public base::RefCountedThreadSafe<
            DeviceProvider,
            content::BrowserThread::DeleteOnUIThread> {
   protected:
    friend class AndroidDeviceManager;

    typedef base::Callback<void(const Devices&)> QueryDevicesCallback;

    virtual void QueryDevices(const QueryDevicesCallback& callback) = 0;

   protected:
    friend struct
        content::BrowserThread::DeleteOnThread<content::BrowserThread::UI>;
    friend class base::DeleteHelper<DeviceProvider>;

    DeviceProvider();
    virtual ~DeviceProvider();
  };

 public:
  static scoped_refptr<AndroidDeviceManager> Create();

  typedef std::vector<scoped_refptr<DeviceProvider> > DeviceProviders;
  typedef base::Callback<void (const std::vector<std::string>&)>
      QueryDevicesCallback;

  void QueryDevices(const DeviceProviders& providers,
                    const QueryDevicesCallback& callback);

  void Stop();

  bool IsConnected(const std::string& serial);

  void QueryDeviceInfo(const std::string& serial,
                       const DeviceInfoCallback& callback);

  void OpenSocket(const std::string& serial,
                  const std::string& socket_name,
                  const SocketCallback& callback);

  void HttpQuery(const std::string& serial,
                 const std::string& socket_name,
                 const std::string& request,
                 const CommandCallback& callback);

  void HttpUpgrade(const std::string& serial,
                   const std::string& socket_name,
                   const std::string& url,
                   const SocketCallback& callback);

 private:
  AndroidDeviceManager();

  friend class base::RefCountedThreadSafe<AndroidDeviceManager>;

  virtual ~AndroidDeviceManager();

  void QueryNextProvider(
      const QueryDevicesCallback& callback,
      const DeviceProviders& providers,
      const Devices& total_devices,
      const Devices& new_devices);

  Device* FindDevice(const std::string& serial);

  typedef std::map<std::string, scoped_refptr<Device> > DeviceMap;
  DeviceMap devices_;

  bool stopped_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_ANDROID_DEVICE_MANAGER_H_
