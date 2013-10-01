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
#include "chrome/browser/devtools/adb/android_usb_device.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "net/socket/tcp_client_socket.h"
#include "ui/gfx/size.h"

template<typename T> struct DefaultSingletonTraits;

namespace base {
class MessageLoop;
class DictionaryValue;
class Thread;
}

namespace content {
class BrowserContext;
}

namespace crypto {
class RSAPrivateKey;
}

class Profile;

// The format used for constructing DevTools server socket names.
extern const char kDevToolsChannelNameFormat[];

typedef base::Callback<void(int, const std::string&)> CommandCallback;
typedef base::Callback<void(int result, net::StreamSocket*)> SocketCallback;

class DevToolsAdbBridge
    : public base::RefCountedThreadSafe<
          DevToolsAdbBridge,
          content::BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<void(int result,
                              const std::string& response)> Callback;

  class Wrapper : public BrowserContextKeyedService {
   public:
    explicit Wrapper(Profile* profile);
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
    friend class DevToolsAdbBridge;

    Factory();
    virtual ~Factory();

    // BrowserContextKeyedServiceFactory overrides:
    virtual BrowserContextKeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const OVERRIDE;
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  class AndroidDevice;

  class RemotePage : public base::RefCounted<RemotePage> {
   public:
    RemotePage(scoped_refptr<DevToolsAdbBridge> bridge,
               scoped_refptr<AndroidDevice> device,
               const std::string& socket,
               const base::DictionaryValue& value);

    std::string id() { return id_; }
    std::string url() { return url_; }
    std::string title() { return title_; }
    std::string description() { return description_; }
    std::string favicon_url() { return favicon_url_; }
    bool attached() { return debug_url_.empty(); }

    bool HasDevToolsWindow();

    void Inspect(Profile* profile);
    void Activate();
    void Close();
    void Reload();

    void SendProtocolCommand(const std::string& method,
                             base::DictionaryValue* params);

   private:
    friend class base::RefCounted<RemotePage>;
    virtual ~RemotePage();

    void RequestActivate(const CommandCallback& callback);

    void InspectOnHandlerThread(
        Profile* profile, int result, const std::string& response);

    void InspectOnUIThread(Profile* profile);

    scoped_refptr<DevToolsAdbBridge> bridge_;
    scoped_refptr<AndroidDevice> device_;
    std::string socket_;
    std::string id_;
    std::string url_;
    std::string title_;
    std::string description_;
    std::string favicon_url_;
    std::string debug_url_;
    std::string frontend_url_;
    std::string agent_id_;
    DISALLOW_COPY_AND_ASSIGN(RemotePage);
  };

  typedef std::vector<scoped_refptr<RemotePage> > RemotePages;

  class RemoteBrowser : public base::RefCounted<RemoteBrowser> {
   public:
    RemoteBrowser(scoped_refptr<DevToolsAdbBridge> bridge,
                  scoped_refptr<AndroidDevice> device,
                  const std::string& socket);

    scoped_refptr<AndroidDevice> device() { return device_; }
    std::string socket() { return socket_; }

    std::string product() { return product_; }
    void set_product(const std::string& product) { product_ = product; }
    std::string version() { return version_; }
    void set_version(const std::string& version) { version_ = version; }
    std::string pid() { return pid_; }
    void set_pid(const std::string& pid) { pid_ = pid; }
    std::string package() { return package_; }
    void set_package(const std::string& package) { package_ = package; }

    RemotePages& pages() { return pages_; }
    void AddPage(scoped_refptr<RemotePage> page) { pages_.push_back(page); }

    void Open(const std::string& url);

   private:
    friend class base::RefCounted<RemoteBrowser>;
    virtual ~RemoteBrowser();

    void PageCreatedOnHandlerThread(
        const std::string& url, int result, const std::string& response);

    void PageCreatedOnUIThread(
        const std::string& response, const std::string& url);

    scoped_refptr<DevToolsAdbBridge> bridge_;
    scoped_refptr<AndroidDevice> device_;
    const std::string socket_;
    std::string product_;
    std::string version_;
    std::string pid_;
    std::string package_;
    RemotePages pages_;

    DISALLOW_COPY_AND_ASSIGN(RemoteBrowser);
  };

  typedef std::vector<scoped_refptr<RemoteBrowser> > RemoteBrowsers;

  class RemoteDevice : public base::RefCounted<RemoteDevice> {
   public:
    explicit RemoteDevice(scoped_refptr<DevToolsAdbBridge> bridge,
                          scoped_refptr<AndroidDevice> device);

    scoped_refptr<AndroidDevice> device() { return device_; }
    std::string serial() { return device_->serial(); }
    std::string model() { return device_->model(); }

    RemoteBrowsers& browsers() { return browsers_; }
    void AddBrowser(scoped_refptr<RemoteBrowser> browser) {
      browsers_.push_back(browser);
    }

    gfx::Size GetScreenSize() { return screen_size_; }
    void SetScreenSize(const gfx::Size& size) { screen_size_ = size; }

   private:
    friend class base::RefCounted<RemoteDevice>;
    virtual ~RemoteDevice();

    scoped_refptr<DevToolsAdbBridge> bridge_;
    scoped_refptr<AndroidDevice> device_;
    RemoteBrowsers browsers_;
    gfx::Size screen_size_;

    DISALLOW_COPY_AND_ASSIGN(RemoteDevice);
  };

  typedef std::vector<scoped_refptr<RemoteDevice> > RemoteDevices;

  class AndroidDevice : public base::RefCounted<AndroidDevice> {
   public:
    explicit AndroidDevice(const std::string& serial);

    virtual void RunCommand(const std::string& command,
                            const CommandCallback& callback) = 0;
    virtual void OpenSocket(const std::string& socket_name,
                            const SocketCallback& callback) = 0;
    void HttpQuery(const std::string& la_name,
                   const std::string& request,
                   const CommandCallback& callback);
    void HttpUpgrade(const std::string& la_name,
                     const std::string& request,
                     const SocketCallback& callback);

    std::string serial() { return serial_; }

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
    std::string model_;

    DISALLOW_COPY_AND_ASSIGN(AndroidDevice);
  };

  typedef std::vector<scoped_refptr<AndroidDevice> > AndroidDevices;
  typedef base::Callback<void(const AndroidDevices&)> AndroidDevicesCallback;

  class Listener {
   public:
    virtual void RemoteDevicesChanged(RemoteDevices* devices) = 0;
   protected:
    virtual ~Listener() {}
  };

  explicit DevToolsAdbBridge(Profile* profile);

  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  base::MessageLoop* GetAdbMessageLoop();

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<DevToolsAdbBridge>;

  class RefCountedAdbThread : public base::RefCounted<RefCountedAdbThread> {
   public:
    static scoped_refptr<RefCountedAdbThread> GetInstance();
    RefCountedAdbThread();
    base::MessageLoop* message_loop();

   private:
    friend class base::RefCounted<RefCountedAdbThread>;
    static DevToolsAdbBridge::RefCountedAdbThread* instance_;
    static void StopThread(base::Thread* thread);

    virtual ~RefCountedAdbThread();
    base::Thread* thread_;
  };

  virtual ~DevToolsAdbBridge();

  void RequestRemoteDevices();
  void ReceivedRemoteDevices(RemoteDevices* devices);

  Profile* profile_;
  scoped_refptr<RefCountedAdbThread> adb_thread_;
  bool has_message_loop_;
  scoped_ptr<crypto::RSAPrivateKey> rsa_key_;
  typedef std::vector<Listener*> Listeners;
  Listeners listeners_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsAdbBridge);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_
