// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/devtools_agent_host.h"
#include "ui/gfx/size.h"

template<typename T> struct DefaultSingletonTraits;

namespace base {
class MessageLoop;
class DictionaryValue;
class ListValue;
class Thread;
}

namespace content {
class BrowserContext;
}

class DevToolsTargetImpl;
class Profile;

class DevToolsAndroidBridge
    : public base::RefCountedThreadSafe<
          DevToolsAndroidBridge,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<void(int result,
                              const std::string& response)> Callback;

  class Wrapper : public KeyedService {
   public:
    explicit Wrapper(content::BrowserContext* context);
    virtual ~Wrapper();

    DevToolsAndroidBridge* Get();
   private:
    scoped_refptr<DevToolsAndroidBridge> bridge_;
  };

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    // Returns singleton instance of DevToolsAndroidBridge.
    static Factory* GetInstance();

    // Returns DevToolsAndroidBridge associated with |profile|.
    static DevToolsAndroidBridge* GetForProfile(Profile* profile);

   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // BrowserContextKeyedServiceFactory overrides:
    virtual KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const OVERRIDE;
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  class RemotePage {
   public:
    virtual ~RemotePage() {}
    virtual DevToolsTargetImpl* GetTarget() = 0;
    virtual std::string GetFrontendURL() = 0;
  };

  typedef base::Callback<void(RemotePage*)> RemotePageCallback;
  typedef base::Callback<void(int, const std::string&)> JsonRequestCallback;
  typedef AndroidDeviceManager::Device Device;
  typedef AndroidDeviceManager::AndroidWebSocket AndroidWebSocket;

  class RemoteBrowser : public base::RefCounted<RemoteBrowser> {
   public:
    RemoteBrowser(scoped_refptr<Device> device,
                  const AndroidDeviceManager::BrowserInfo& browser_info);

    std::string serial() { return device_->serial(); }
    std::string socket() { return socket_; }

    std::string display_name() { return display_name_; }
    void set_display_name(const std::string& name) { display_name_ = name; }

    std::string version() { return version_; }
    void set_version(const std::string& version) { version_ = version; }

    bool IsChrome() const;
    bool IsWebView() const;

    typedef std::vector<int> ParsedVersion;
    ParsedVersion GetParsedVersion() const;

    std::vector<RemotePage*> CreatePages();
    void SetPageDescriptors(const base::ListValue&);

    void SendJsonRequest(const std::string& request,
                         const JsonRequestCallback& callback);

    void SendProtocolCommand(const std::string& debug_url,
                             const std::string& method,
                             base::DictionaryValue* params,
                             const base::Closure callback);

    void Open(const std::string& url,
              const RemotePageCallback& callback);

    scoped_refptr<content::DevToolsAgentHost> GetAgentHost();

    AndroidWebSocket* CreateWebSocket(
        const std::string& url,
        DevToolsAndroidBridge::AndroidWebSocket::Delegate* delegate);

   private:
    friend class base::RefCounted<RemoteBrowser>;
    virtual ~RemoteBrowser();

    void InnerOpen(const std::string& url,
                   const JsonRequestCallback& callback);

    void PageCreatedOnUIThread(
        const JsonRequestCallback& callback,
        const std::string& url, int result, const std::string& response);

    void NavigatePageOnUIThread(const JsonRequestCallback& callback,
        int result,
        const std::string& response,
        const std::string& url);

    void RespondToOpenOnUIThread(
        const DevToolsAndroidBridge::RemotePageCallback& callback,
        int result,
        const std::string& response);

    scoped_refptr<Device> device_;
    const std::string socket_;
    std::string display_name_;
    const AndroidDeviceManager::BrowserInfo::Type type_;
    std::string version_;
    scoped_ptr<base::ListValue> page_descriptors_;

    DISALLOW_COPY_AND_ASSIGN(RemoteBrowser);
  };

  typedef std::vector<scoped_refptr<RemoteBrowser> > RemoteBrowsers;

  class RemoteDevice : public base::RefCounted<RemoteDevice> {
   public:
    RemoteDevice(scoped_refptr<Device> device,
                 const AndroidDeviceManager::DeviceInfo& device_info);

    std::string serial() { return device_->serial(); }
    std::string model() { return model_; }
    bool is_connected() { return connected_; }
    RemoteBrowsers& browsers() { return browsers_; }
    gfx::Size screen_size() { return screen_size_; }

    void OpenSocket(const std::string& socket_name,
                    const AndroidDeviceManager::SocketCallback& callback);

   private:
    friend class base::RefCounted<RemoteDevice>;
    virtual ~RemoteDevice();

    scoped_refptr<Device> device_;
    std::string model_;
    bool connected_;
    RemoteBrowsers browsers_;
    gfx::Size screen_size_;

    DISALLOW_COPY_AND_ASSIGN(RemoteDevice);
  };

  typedef std::vector<scoped_refptr<RemoteDevice> > RemoteDevices;

  class DeviceListListener {
   public:
    virtual void DeviceListChanged(const RemoteDevices& devices) = 0;
   protected:
    virtual ~DeviceListListener() {}
  };

  explicit DevToolsAndroidBridge(Profile* profile);
  void AddDeviceListListener(DeviceListListener* listener);
  void RemoveDeviceListListener(DeviceListListener* listener);

  class DeviceCountListener {
   public:
    virtual void DeviceCountChanged(int count) = 0;
   protected:
    virtual ~DeviceCountListener() {}
  };

  void AddDeviceCountListener(DeviceCountListener* listener);
  void RemoveDeviceCountListener(DeviceCountListener* listener);

  void set_device_providers_for_test(
      const AndroidDeviceManager::DeviceProviders& device_providers) {
    device_manager_->SetDeviceProviders(device_providers);
  }

  void set_task_scheduler_for_test(
      base::Callback<void(const base::Closure&)> scheduler) {
    task_scheduler_ = scheduler;
  }

  static bool HasDevToolsWindow(const std::string& agent_id);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<DevToolsAndroidBridge>;

  virtual ~DevToolsAndroidBridge();

  void StartDeviceListPolling();
  void StopDeviceListPolling();
  void RequestDeviceList(
      const base::Callback<void(const RemoteDevices&)>& callback);
  void ReceivedDeviceList(const RemoteDevices& devices);
  void StartDeviceCountPolling();
  void StopDeviceCountPolling();
  void RequestDeviceCount(const base::Callback<void(int)>& callback);
  void ReceivedDeviceCount(int count);

  static void ScheduleTaskDefault(const base::Closure& task);

  void CreateDeviceProviders();

  Profile* profile_;
  scoped_refptr<AndroidDeviceManager> device_manager_;
  RemoteDevices devices_;

  typedef std::vector<DeviceListListener*> DeviceListListeners;
  DeviceListListeners device_list_listeners_;
  base::CancelableCallback<void(const RemoteDevices&)> device_list_callback_;

  typedef std::vector<DeviceCountListener*> DeviceCountListeners;
  DeviceCountListeners device_count_listeners_;
  base::CancelableCallback<void(int)> device_count_callback_;
  base::Callback<void(const base::Closure&)> task_scheduler_;

  PrefChangeRegistrar pref_change_registrar_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsAndroidBridge);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_
