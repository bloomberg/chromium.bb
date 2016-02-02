// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/device/android_device_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/devtools_agent_host.h"
#include "ui/gfx/geometry/size.h"

namespace base {
template<typename T> struct DefaultSingletonTraits;

class MessageLoop;
class DictionaryValue;
class ListValue;
class Thread;
}  // namespace base

namespace content {
class BrowserContext;
}

class DevToolsTargetImpl;
class PortForwardingController;
class Profile;

class DevToolsAndroidBridge : public KeyedService {
 public:
  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    // Returns singleton instance of DevToolsAndroidBridge.
    static Factory* GetInstance();

    // Returns DevToolsAndroidBridge associated with |profile|.
    static DevToolsAndroidBridge* GetForProfile(Profile* profile);

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory overrides:
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const override;
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  typedef std::pair<std::string, std::string> BrowserId;

  class RemotePage : public base::RefCounted<RemotePage> {
   public:
    const std::string& serial() { return browser_id_.first; }
    const std::string& socket() { return browser_id_.second; }
    const std::string& frontend_url() { return frontend_url_; }

   private:
    friend class base::RefCounted<RemotePage>;
    friend class DevToolsAndroidBridge;

    RemotePage(const BrowserId& browser_id, const base::DictionaryValue& dict);

    virtual ~RemotePage();

    BrowserId browser_id_;
    std::string frontend_url_;
    scoped_ptr<base::DictionaryValue> dict_;

    DISALLOW_COPY_AND_ASSIGN(RemotePage);
  };

  typedef std::vector<scoped_refptr<RemotePage> > RemotePages;
  typedef base::Callback<void(int, const std::string&)> JsonRequestCallback;

  class RemoteBrowser : public base::RefCounted<RemoteBrowser> {
   public:
    const std::string& serial() { return browser_id_.first; }
    const std::string& socket() { return browser_id_.second; }
    const std::string& display_name() { return display_name_; }
    const std::string& user() { return user_; }
    const std::string& version() { return version_; }
    const RemotePages& pages() { return pages_; }

    bool IsChrome();
    std::string GetId();

    typedef std::vector<int> ParsedVersion;
    ParsedVersion GetParsedVersion();

   private:
    friend class base::RefCounted<RemoteBrowser>;
    friend class DevToolsAndroidBridge;

    RemoteBrowser(const std::string& serial,
                  const AndroidDeviceManager::BrowserInfo& browser_info);

    virtual ~RemoteBrowser();

    BrowserId browser_id_;
    std::string display_name_;
    std::string user_;
    AndroidDeviceManager::BrowserInfo::Type type_;
    std::string version_;
    RemotePages pages_;

    DISALLOW_COPY_AND_ASSIGN(RemoteBrowser);
  };

  typedef std::vector<scoped_refptr<RemoteBrowser> > RemoteBrowsers;

  class RemoteDevice : public base::RefCounted<RemoteDevice> {
   public:
    std::string serial() { return serial_; }
    std::string model() { return model_; }
    bool is_connected() { return connected_; }
    RemoteBrowsers& browsers() { return browsers_; }
    gfx::Size screen_size() { return screen_size_; }

   private:
    friend class base::RefCounted<RemoteDevice>;
    friend class DevToolsAndroidBridge;

    RemoteDevice(const std::string& serial,
                 const AndroidDeviceManager::DeviceInfo& device_info);

    virtual ~RemoteDevice();

    std::string serial_;
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

  typedef int PortStatus;
  typedef std::map<int, PortStatus> PortStatusMap;
  typedef std::pair<scoped_refptr<RemoteBrowser>, PortStatusMap>
      BrowserStatus;
  typedef std::vector<BrowserStatus> ForwardingStatus;

  class PortForwardingListener {
   public:
    typedef DevToolsAndroidBridge::PortStatusMap PortStatusMap;
    typedef DevToolsAndroidBridge::BrowserStatus BrowserStatus;
    typedef DevToolsAndroidBridge::ForwardingStatus ForwardingStatus;

    virtual void PortStatusChanged(const ForwardingStatus&) = 0;
   protected:
    virtual ~PortForwardingListener() {}
  };

  void AddPortForwardingListener(PortForwardingListener* listener);
  void RemovePortForwardingListener(PortForwardingListener* listener);

  void set_device_providers_for_test(
      const AndroidDeviceManager::DeviceProviders& device_providers) {
    device_manager_->SetDeviceProviders(device_providers);
  }

  void set_task_scheduler_for_test(
      base::Callback<void(const base::Closure&)> scheduler) {
    task_scheduler_ = scheduler;
  }

  bool HasDevToolsWindow(const std::string& agent_id);

  // Creates new target instance owned by caller.
  DevToolsTargetImpl* CreatePageTarget(scoped_refptr<RemotePage> browser);

  typedef base::Callback<void(scoped_refptr<RemotePage>)> RemotePageCallback;
  void OpenRemotePage(scoped_refptr<RemoteBrowser> browser,
                      const std::string& url);

  scoped_refptr<content::DevToolsAgentHost> GetBrowserAgentHost(
      scoped_refptr<RemoteBrowser> browser);

  void SendJsonRequest(const std::string& browser_id_str,
                       const std::string& url,
                       const JsonRequestCallback& callback);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<DevToolsAndroidBridge>;

  friend class PortForwardingController;

  class AgentHostDelegate;
  class DiscoveryRequest;
  class RemotePageTarget;

  ~DevToolsAndroidBridge() override;

  void StartDeviceListPolling();
  void StopDeviceListPolling();
  bool NeedsDeviceListPolling();

  typedef std::pair<scoped_refptr<AndroidDeviceManager::Device>,
                    scoped_refptr<RemoteDevice>> CompleteDevice;
  typedef std::vector<CompleteDevice> CompleteDevices;
  typedef base::Callback<void(const CompleteDevices&)> DeviceListCallback;

  void RequestDeviceList(const DeviceListCallback& callback);
  void ReceivedDeviceList(const CompleteDevices& complete_devices);

  void StartDeviceCountPolling();
  void StopDeviceCountPolling();
  void RequestDeviceCount(const base::Callback<void(int)>& callback);
  void ReceivedDeviceCount(int count);

  static void ScheduleTaskDefault(const base::Closure& task);

  void CreateDeviceProviders();

  void SendJsonRequest(const BrowserId& browser_id,
                       const std::string& url,
                       const JsonRequestCallback& callback);

  void SendProtocolCommand(const BrowserId& browser_id,
                           const std::string& target_path,
                           const std::string& method,
                           scoped_ptr<base::DictionaryValue> params,
                           const base::Closure callback);

  scoped_refptr<AndroidDeviceManager::Device> FindDevice(
      const std::string& serial);

  base::WeakPtr<DevToolsAndroidBridge> AsWeakPtr() {
      return weak_factory_.GetWeakPtr();
  }

  Profile* const profile_;
  const scoped_ptr<AndroidDeviceManager> device_manager_;

  typedef std::map<std::string, scoped_refptr<AndroidDeviceManager::Device>>
      DeviceMap;
  DeviceMap device_map_;

  typedef std::map<std::string, AgentHostDelegate*> AgentHostDelegates;
  AgentHostDelegates host_delegates_;

  typedef std::vector<DeviceListListener*> DeviceListListeners;
  DeviceListListeners device_list_listeners_;
  base::CancelableCallback<void(const CompleteDevices&)> device_list_callback_;

  typedef std::vector<DeviceCountListener*> DeviceCountListeners;
  DeviceCountListeners device_count_listeners_;
  base::CancelableCallback<void(int)> device_count_callback_;
  base::Callback<void(const base::Closure&)> task_scheduler_;

  typedef std::vector<PortForwardingListener*> PortForwardingListeners;
  PortForwardingListeners port_forwarding_listeners_;
  scoped_ptr<PortForwardingController> port_forwarding_controller_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<DevToolsAndroidBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAndroidBridge);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_DEVTOOLS_ANDROID_BRIDGE_H_
