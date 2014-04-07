// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/devtools/android_device.h"
#include "chrome/browser/devtools/refcounted_adb_thread.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
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

namespace crypto {
class RSAPrivateKey;
}

class DevToolsTargetImpl;
class Profile;

// The format used for constructing DevTools server socket names.
extern const char kDevToolsChannelNameFormat[];

class DevToolsAdbBridge
    : public base::RefCountedThreadSafe<
          DevToolsAdbBridge,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<void(int result,
                              const std::string& response)> Callback;
  typedef std::vector<scoped_refptr<AndroidDeviceProvider> > DeviceProviders;

  class Wrapper : public KeyedService {
   public:
    explicit Wrapper(content::BrowserContext* context);
    virtual ~Wrapper();

    DevToolsAdbBridge* Get();
   private:
    scoped_refptr<DevToolsAdbBridge> bridge_;
  };

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    // Returns singleton instance of DevToolsAdbBridge.
    static Factory* GetInstance();

    // Returns DevToolsAdbBridge associated with |profile|.
    static DevToolsAdbBridge* GetForProfile(Profile* profile);

   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // BrowserContextKeyedServiceFactory overrides:
    virtual KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const OVERRIDE;
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

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

    AdbWebSocket() {}

    virtual void Disconnect() = 0;

    virtual void SendFrame(const std::string& message) = 0;

   protected:
    virtual ~AdbWebSocket() {}

   private:
    friend class base::RefCountedThreadSafe<AdbWebSocket>;

    DISALLOW_COPY_AND_ASSIGN(AdbWebSocket);
  };

  class RemoteBrowser : public base::RefCounted<RemoteBrowser> {
   public:
    RemoteBrowser(
        scoped_refptr<DevToolsAdbBridge> adb_bridge,
        const std::string& serial,
        const std::string& socket);

    std::string serial() { return serial_; }
    std::string socket() { return socket_; }

    std::string display_name() { return display_name_; }
    void set_display_name(const std::string& name) { display_name_ = name; }

    std::string version() { return version_; }
    void set_version(const std::string& version) { version_ = version; }

    bool IsChrome() const;

    typedef std::vector<int> ParsedVersion;
    ParsedVersion GetParsedVersion() const;

    std::vector<DevToolsTargetImpl*> CreatePageTargets();
    void SetPageDescriptors(const base::ListValue&);

    typedef base::Callback<void(int, const std::string&)> JsonRequestCallback;
    void SendJsonRequest(const std::string& request,
                         const JsonRequestCallback& callback);
    void SendProtocolCommand(const std::string& debug_url,
                             const std::string& method,
                             base::DictionaryValue* params);

    void Open(const std::string& url);

    scoped_refptr<AdbWebSocket> CreateWebSocket(
        const std::string& url,
        DevToolsAdbBridge::AdbWebSocket::Delegate* delegate);

   private:
    friend class base::RefCounted<RemoteBrowser>;
    virtual ~RemoteBrowser();

    void PageCreatedOnHandlerThread(
        const std::string& url, int result, const std::string& response);

    void PageCreatedOnUIThread(
        const std::string& response, const std::string& url);

    scoped_refptr<DevToolsAdbBridge> adb_bridge_;
    const std::string serial_;
    const std::string socket_;
    std::string display_name_;
    std::string version_;
    scoped_ptr<base::ListValue> page_descriptors_;

    DISALLOW_COPY_AND_ASSIGN(RemoteBrowser);
  };

  typedef std::vector<scoped_refptr<RemoteBrowser> > RemoteBrowsers;

  class RemoteDevice : public base::RefCounted<RemoteDevice> {
   public:
    RemoteDevice(scoped_refptr<DevToolsAdbBridge> adb_bridge,
                 const std::string& serial,
                 const std::string& model,
                 bool connected);

    std::string serial() { return serial_; }
    std::string model() { return model_; }
    bool is_connected() { return connected_; }
    void AddBrowser(scoped_refptr<RemoteBrowser> browser);

    RemoteBrowsers& browsers() { return browsers_; }
    gfx::Size screen_size() { return screen_size_; }
    void set_screen_size(const gfx::Size& size) { screen_size_ = size; }

    void OpenSocket(const std::string& socket_name,
                    const AndroidDeviceManager::SocketCallback& callback);

   private:
    friend class base::RefCounted<RemoteDevice>;
    virtual ~RemoteDevice();

    scoped_refptr<DevToolsAdbBridge> adb_bridge_;
    std::string serial_;
    std::string model_;
    bool connected_;
    RemoteBrowsers browsers_;
    gfx::Size screen_size_;

    DISALLOW_COPY_AND_ASSIGN(RemoteDevice);
  };

  typedef std::vector<scoped_refptr<RemoteDevice> > RemoteDevices;

  class Listener {
   public:
    virtual void RemoteDevicesChanged(RemoteDevices* devices) = 0;
   protected:
    virtual ~Listener() {}
  };

  explicit DevToolsAdbBridge(Profile* profile);
  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  void set_device_providers(DeviceProviders device_providers) {
    device_providers_ = device_providers;
  }

  static bool HasDevToolsWindow(const std::string& agent_id);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<DevToolsAdbBridge>;

  virtual ~DevToolsAdbBridge();

  base::MessageLoop* device_message_loop() {
    return adb_thread_->message_loop();
  }

  AndroidDeviceManager* device_manager() {
    return device_manager_.get();
  }

  void CreatedDeviceManager(scoped_refptr<AndroidDeviceManager> device_manager);
  void RequestRemoteDevices();
  void ReceivedRemoteDevices(RemoteDevices* devices);
  void CreateDeviceProviders();

  Profile* profile_;
  scoped_refptr<RefCountedAdbThread> adb_thread_;
  scoped_refptr<AndroidDeviceManager> device_manager_;
  typedef std::vector<Listener*> Listeners;
  Listeners listeners_;
  DeviceProviders device_providers_;
  PrefChangeRegistrar pref_change_registrar_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsAdbBridge);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_
