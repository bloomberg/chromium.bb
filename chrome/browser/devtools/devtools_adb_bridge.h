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
    RemotePage(scoped_refptr<AndroidDevice> device,
               const std::string& package,
               const std::string& socket,
               const base::DictionaryValue& value);

    scoped_refptr<AndroidDevice> device() { return device_; }
    std::string serial() { return device_->serial(); }
    std::string model() { return device_->model(); }
    std::string package() { return package_; }
    std::string socket() { return socket_; }
    std::string id() { return id_; }
    std::string url() { return url_; }
    std::string title() { return title_; }
    std::string description() { return description_; }
    std::string favicon_url() { return favicon_url_; }
    std::string debug_url() { return debug_url_; }
    std::string frontend_url() { return frontend_url_; }

   private:
    friend class base::RefCounted<RemotePage>;
    virtual ~RemotePage();
    scoped_refptr<AndroidDevice> device_;
    std::string package_;
    std::string socket_;
    std::string id_;
    std::string url_;
    std::string title_;
    std::string description_;
    std::string favicon_url_;
    std::string debug_url_;
    std::string frontend_url_;
    DISALLOW_COPY_AND_ASSIGN(RemotePage);
  };

  typedef std::vector<scoped_refptr<RemotePage> > RemotePages;
  typedef base::Callback<void(int, RemotePages*)> PagesCallback;

  class AndroidDevice : public base::RefCounted<AndroidDevice> {
   public:
    explicit AndroidDevice(const std::string& serial);

    virtual void RunCommand(const std::string& command,
                            const CommandCallback& callback) = 0;
    virtual void OpenSocket(const std::string& socket_name,
                            const SocketCallback& callback) = 0;
    virtual void HttpQuery(const std::string& la_name,
                           const std::string& request,
                           const CommandCallback& callback);
    virtual void HttpQuery(const std::string& la_name,
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
    virtual void RemotePagesChanged(RemotePages* pages) = 0;
   protected:
    virtual ~Listener() {}
  };

  explicit DevToolsAdbBridge(Profile* profile);

  void EnumerateUsbDevices(const AndroidDevicesCallback& callback);
  void EnumerateAdbDevices(const AndroidDevicesCallback& callback);

  void Attach(const std::string& page_id);

  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  base::MessageLoop* GetAdbMessageLoop();

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<DevToolsAdbBridge>;
  friend class AdbWebSocket;
  friend class AgentHostDelegate;

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
  void ReceivedUsbDevices(const AndroidDevicesCallback& callback,
                          const AndroidUsbDevices& usb_devices);
  void ReceivedAdbDevices(const AndroidDevicesCallback& callback,
                          int result,
                          const std::string& response);

  void RequestPages();
  void ReceivedPages(int result, RemotePages* pages);

  Profile* profile_;
  scoped_refptr<RefCountedAdbThread> adb_thread_;
  bool has_message_loop_;
  scoped_ptr<crypto::RSAPrivateKey> rsa_key_;
  scoped_ptr<RemotePages> pages_;
  typedef std::vector<Listener*> Listeners;
  Listeners listeners_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsAdbBridge);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_ADB_BRIDGE_H_
