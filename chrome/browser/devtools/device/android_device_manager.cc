// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/android_device_manager.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

using content::BrowserThread;

namespace {

const char kDevToolsAdbBridgeThreadName[] = "Chrome_DevToolsADBThread";

const int kBufferSize = 16 * 1024;

static const char kModelOffline[] = "Offline";

static const char kHttpGetRequest[] = "GET %s HTTP/1.1\r\n\r\n";

static const char kWebSocketUpgradeRequest[] = "GET %s HTTP/1.1\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";

static void PostDeviceInfoCallback(
    scoped_refptr<base::MessageLoopProxy> response_message_loop,
    const AndroidDeviceManager::DeviceInfoCallback& callback,
    const AndroidDeviceManager::DeviceInfo& device_info) {
  response_message_loop->PostTask(FROM_HERE, base::Bind(callback, device_info));
}

static void PostCommandCallback(
    scoped_refptr<base::MessageLoopProxy> response_message_loop,
    const AndroidDeviceManager::CommandCallback& callback,
    int result,
    const std::string& response) {
  response_message_loop->PostTask(FROM_HERE,
                                  base::Bind(callback, result, response));
}

static void PostSocketCallback(
    scoped_refptr<base::MessageLoopProxy> response_message_loop,
    const AndroidDeviceManager::SocketCallback& callback,
    int result,
    net::StreamSocket* socket) {
  response_message_loop->PostTask(FROM_HERE,
                                  base::Bind(callback, result, socket));
}

class HttpRequest {
 public:
  typedef AndroidDeviceManager::CommandCallback CommandCallback;
  typedef AndroidDeviceManager::SocketCallback SocketCallback;

  static void CommandRequest(const std::string& request,
                           const CommandCallback& callback,
                           int result,
                           net::StreamSocket* socket) {
    if (result != net::OK) {
      callback.Run(result, std::string());
      return;
    }
    new HttpRequest(socket, request, callback);
  }

  static void SocketRequest(const std::string& request,
                          const SocketCallback& callback,
                          int result,
                          net::StreamSocket* socket) {
    if (result != net::OK) {
      callback.Run(result, NULL);
      return;
    }
    new HttpRequest(socket, request, callback);
  }

 private:
  HttpRequest(net::StreamSocket* socket,
              const std::string& request,
              const CommandCallback& callback)
      : socket_(socket), command_callback_(callback), body_pos_(0) {
    SendRequest(request);
  }

  HttpRequest(net::StreamSocket* socket,
                      const std::string& request,
                      const SocketCallback& callback)
    : socket_(socket),
      socket_callback_(callback),
      body_pos_(0) {
    SendRequest(request);
  }

  ~HttpRequest() {
  }

  void SendRequest(const std::string& request) {
    scoped_refptr<net::StringIOBuffer> request_buffer =
        new net::StringIOBuffer(request);

    int result = socket_->Write(
        request_buffer.get(),
        request_buffer->size(),
        base::Bind(&HttpRequest::ReadResponse, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      ReadResponse(result);
  }

  void ReadResponse(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    scoped_refptr<net::IOBuffer> response_buffer =
        new net::IOBuffer(kBufferSize);

    result = socket_->Read(
        response_buffer.get(),
        kBufferSize,
        base::Bind(&HttpRequest::OnResponseData, base::Unretained(this),
                  response_buffer,
                  -1));
    if (result != net::ERR_IO_PENDING)
      OnResponseData(response_buffer, -1, result);
  }

  void OnResponseData(scoped_refptr<net::IOBuffer> response_buffer,
                      int bytes_total,
                      int result) {
    if (!CheckNetResultOrDie(result))
      return;
    if (result == 0) {
      CheckNetResultOrDie(net::ERR_CONNECTION_CLOSED);
      return;
    }

    response_ += std::string(response_buffer->data(), result);
    int expected_length = 0;
    if (bytes_total < 0) {
      // TODO(kaznacheev): Use net::HttpResponseHeader to parse the header.
      size_t content_pos = response_.find("Content-Length:");
      if (content_pos != std::string::npos) {
        size_t endline_pos = response_.find("\n", content_pos);
        if (endline_pos != std::string::npos) {
          std::string len = response_.substr(content_pos + 15,
                                             endline_pos - content_pos - 15);
          base::TrimWhitespace(len, base::TRIM_ALL, &len);
          if (!base::StringToInt(len, &expected_length)) {
            CheckNetResultOrDie(net::ERR_FAILED);
            return;
          }
        }
      }

      body_pos_ = response_.find("\r\n\r\n");
      if (body_pos_ != std::string::npos) {
        body_pos_ += 4;
        bytes_total = body_pos_ + expected_length;
      }
    }

    if (bytes_total == static_cast<int>(response_.length())) {
      if (!command_callback_.is_null())
        command_callback_.Run(net::OK, response_.substr(body_pos_));
      else
        socket_callback_.Run(net::OK, socket_.release());
      delete this;
      return;
    }

    result = socket_->Read(
        response_buffer.get(),
        kBufferSize,
        base::Bind(&HttpRequest::OnResponseData,
                   base::Unretained(this),
                   response_buffer,
                   bytes_total));
    if (result != net::ERR_IO_PENDING)
      OnResponseData(response_buffer, bytes_total, result);
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    if (!command_callback_.is_null())
      command_callback_.Run(result, std::string());
    else
      socket_callback_.Run(result, NULL);
    delete this;
    return false;
  }

  scoped_ptr<net::StreamSocket> socket_;
  std::string response_;
  AndroidDeviceManager::CommandCallback command_callback_;
  AndroidDeviceManager::SocketCallback socket_callback_;
  size_t body_pos_;
};

class DevicesRequest : public base::RefCountedThreadSafe<DevicesRequest> {
 public:
  typedef AndroidDeviceManager::DeviceInfo DeviceInfo;
  typedef AndroidDeviceManager::DeviceProvider DeviceProvider;
  typedef AndroidDeviceManager::DeviceProviders DeviceProviders;
  typedef AndroidDeviceManager::DeviceDescriptors DeviceDescriptors;
  typedef base::Callback<void(DeviceDescriptors*)>
      DescriptorsCallback;

  static void Start(scoped_refptr<base::MessageLoopProxy> device_message_loop,
                    const DeviceProviders& providers,
                    const DescriptorsCallback& callback) {
    // Don't keep counted reference on calling thread;
    DevicesRequest* request = new DevicesRequest(callback);
    // Avoid destruction while sending requests
    request->AddRef();
    for (DeviceProviders::const_iterator it = providers.begin();
         it != providers.end(); ++it) {
      device_message_loop->PostTask(
          FROM_HERE,
          base::Bind(
              &DeviceProvider::QueryDevices,
              *it,
              base::Bind(&DevicesRequest::ProcessSerials, request, *it)));
    }
    device_message_loop->ReleaseSoon(FROM_HERE, request);
  }

 private:
  explicit DevicesRequest(const DescriptorsCallback& callback)
      : response_message_loop_(base::MessageLoopProxy::current()),
        callback_(callback),
        descriptors_(new DeviceDescriptors()) {
  }

  friend class base::RefCountedThreadSafe<DevicesRequest>;
  ~DevicesRequest() {
    response_message_loop_->PostTask(FROM_HERE,
        base::Bind(callback_, descriptors_.release()));
  }

  typedef std::vector<std::string> Serials;

  void ProcessSerials(scoped_refptr<DeviceProvider> provider,
                      const Serials& serials) {
    for (Serials::const_iterator it = serials.begin(); it != serials.end();
         ++it) {
      descriptors_->resize(descriptors_->size() + 1);
      descriptors_->back().provider = provider;
      descriptors_->back().serial = *it;
    }
  }

  scoped_refptr<base::MessageLoopProxy> response_message_loop_;
  DescriptorsCallback callback_;
  scoped_ptr<DeviceDescriptors> descriptors_;
};

void ReleaseDeviceAndProvider(
    AndroidDeviceManager::DeviceProvider* provider,
    const std::string& serial) {
  provider->ReleaseDevice(serial);
  provider->Release();
}

} // namespace

AndroidDeviceManager::BrowserInfo::BrowserInfo()
    : type(kTypeOther) {
}

AndroidDeviceManager::DeviceInfo::DeviceInfo()
    : model(kModelOffline), connected(false) {
}

AndroidDeviceManager::DeviceInfo::~DeviceInfo() {
}

AndroidDeviceManager::DeviceDescriptor::DeviceDescriptor() {
}

AndroidDeviceManager::DeviceDescriptor::~DeviceDescriptor() {
}

void AndroidDeviceManager::DeviceProvider::SendJsonRequest(
    const std::string& serial,
    const std::string& socket_name,
    const std::string& request,
    const CommandCallback& callback) {
  OpenSocket(serial,
             socket_name,
             base::Bind(&HttpRequest::CommandRequest,
                        base::StringPrintf(kHttpGetRequest, request.c_str()),
                        callback));
}

void AndroidDeviceManager::DeviceProvider::HttpUpgrade(
    const std::string& serial,
    const std::string& socket_name,
    const std::string& url,
    const SocketCallback& callback) {
  OpenSocket(
      serial,
      socket_name,
      base::Bind(&HttpRequest::SocketRequest,
                 base::StringPrintf(kWebSocketUpgradeRequest, url.c_str()),
                 callback));
}

void AndroidDeviceManager::DeviceProvider::ReleaseDevice(
    const std::string& serial) {
}

AndroidDeviceManager::DeviceProvider::DeviceProvider() {
}

AndroidDeviceManager::DeviceProvider::~DeviceProvider() {
}

void AndroidDeviceManager::Device::QueryDeviceInfo(
    const DeviceInfoCallback& callback) {
  device_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&DeviceProvider::QueryDeviceInfo,
                 provider_,
                 serial_,
                 base::Bind(&PostDeviceInfoCallback,
                            base::MessageLoopProxy::current(),
                            callback)));
}

void AndroidDeviceManager::Device::OpenSocket(const std::string& socket_name,
                                              const SocketCallback& callback) {
  device_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&DeviceProvider::OpenSocket,
                 provider_,
                 serial_,
                 socket_name,
                 callback));
}

void AndroidDeviceManager::Device::SendJsonRequest(
    const std::string& socket_name,
    const std::string& request,
    const CommandCallback& callback) {
  device_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&DeviceProvider::SendJsonRequest,
                 provider_,
                 serial_,
                 socket_name,
                 request,
                 base::Bind(&PostCommandCallback,
                            base::MessageLoopProxy::current(),
                            callback)));
}

void AndroidDeviceManager::Device::HttpUpgrade(const std::string& socket_name,
                                               const std::string& url,
                                               const SocketCallback& callback) {
  device_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&DeviceProvider::HttpUpgrade,
                 provider_,
                 serial_,
                 socket_name,
                 url,
                 base::Bind(&PostSocketCallback,
                            base::MessageLoopProxy::current(),
                            callback)));
}

AndroidDeviceManager::Device::Device(
    scoped_refptr<base::MessageLoopProxy> device_message_loop,
    scoped_refptr<DeviceProvider> provider,
    const std::string& serial)
    : device_message_loop_(device_message_loop),
      provider_(provider),
      serial_(serial),
      weak_factory_(this) {
}

AndroidDeviceManager::Device::~Device() {
  provider_->AddRef();
  DeviceProvider* raw_ptr = provider_.get();
  provider_ = NULL;
  device_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReleaseDeviceAndProvider,
                 base::Unretained(raw_ptr),
                 serial_));
}

AndroidDeviceManager::HandlerThread*
AndroidDeviceManager::HandlerThread::instance_ = NULL;

// static
scoped_refptr<AndroidDeviceManager::HandlerThread>
AndroidDeviceManager::HandlerThread::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!instance_)
    new HandlerThread();
  return instance_;
}

AndroidDeviceManager::HandlerThread::HandlerThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  instance_ = this;
  thread_ = new base::Thread(kDevToolsAdbBridgeThreadName);
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options)) {
    delete thread_;
    thread_ = NULL;
  }
}

scoped_refptr<base::MessageLoopProxy>
AndroidDeviceManager::HandlerThread::message_loop() {
  return thread_ ? thread_->message_loop_proxy() : NULL;
}

// static
void AndroidDeviceManager::HandlerThread::StopThread(
    base::Thread* thread) {
  thread->Stop();
}

AndroidDeviceManager::HandlerThread::~HandlerThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  instance_ = NULL;
  if (!thread_)
    return;
  // Shut down thread on FILE thread to join into IO.
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&HandlerThread::StopThread, thread_));
}

// static
scoped_refptr<AndroidDeviceManager> AndroidDeviceManager::Create() {
  return new AndroidDeviceManager();
}

void AndroidDeviceManager::SetDeviceProviders(
    const DeviceProviders& providers) {
  for (DeviceProviders::iterator it = providers_.begin();
      it != providers_.end(); ++it) {
    (*it)->AddRef();
    DeviceProvider* raw_ptr = it->get();
    *it = NULL;
    handler_thread_->message_loop()->ReleaseSoon(FROM_HERE, raw_ptr);
  }
  providers_ = providers;
}

void AndroidDeviceManager::QueryDevices(const DevicesCallback& callback) {
  DevicesRequest::Start(handler_thread_->message_loop(),
                        providers_,
                        base::Bind(&AndroidDeviceManager::UpdateDevices,
                                   this,
                                   callback));
}

AndroidDeviceManager::AndroidDeviceManager()
    : handler_thread_(HandlerThread::GetInstance()) {
}

AndroidDeviceManager::~AndroidDeviceManager() {
  SetDeviceProviders(DeviceProviders());
}

void AndroidDeviceManager::UpdateDevices(
    const DevicesCallback& callback,
    DeviceDescriptors* descriptors_raw) {
  scoped_ptr<DeviceDescriptors> descriptors(descriptors_raw);
  Devices response;
  DeviceWeakMap new_devices;
  for (DeviceDescriptors::const_iterator it = descriptors->begin();
       it != descriptors->end();
       ++it) {
    DeviceWeakMap::iterator found = devices_.find(it->serial);
    scoped_refptr<Device> device;
    if (found == devices_.end() || !found->second
        || found->second->provider_ != it->provider) {
      device = new Device(handler_thread_->message_loop(),
          it->provider, it->serial);
    } else {
      device = found->second.get();
    }
    response.push_back(device);
    new_devices[it->serial] = device->weak_factory_.GetWeakPtr();
  }
  devices_.swap(new_devices);
  callback.Run(response);
}
