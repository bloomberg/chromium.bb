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
    "%s"
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

static void PostHttpUpgradeCallback(
    scoped_refptr<base::MessageLoopProxy> response_message_loop,
    const AndroidDeviceManager::HttpUpgradeCallback& callback,
    int result,
    const std::string& extensions,
    scoped_ptr<net::StreamSocket> socket) {
  response_message_loop->PostTask(
      FROM_HERE,
      base::Bind(callback, result, extensions, base::Passed(&socket)));
}

class HttpRequest {
 public:
  typedef AndroidDeviceManager::CommandCallback CommandCallback;
  typedef AndroidDeviceManager::HttpUpgradeCallback HttpUpgradeCallback;

  static void CommandRequest(const std::string& request,
                             const CommandCallback& callback,
                             int result,
                             scoped_ptr<net::StreamSocket> socket) {
    if (result != net::OK) {
      callback.Run(result, std::string());
      return;
    }
    new HttpRequest(socket.Pass(), request, callback);
  }

  static void HttpUpgradeRequest(const std::string& request,
                                 const HttpUpgradeCallback& callback,
                                 int result,
                                 scoped_ptr<net::StreamSocket> socket) {
    if (result != net::OK) {
      callback.Run(result, "", make_scoped_ptr<net::StreamSocket>(nullptr));
      return;
    }
    new HttpRequest(socket.Pass(), request, callback);
  }

 private:
  HttpRequest(scoped_ptr<net::StreamSocket> socket,
              const std::string& request,
              const CommandCallback& callback)
      : socket_(socket.Pass()),
        command_callback_(callback),
        body_pos_(0) {
    SendRequest(request);
  }

  HttpRequest(scoped_ptr<net::StreamSocket> socket,
              const std::string& request,
              const HttpUpgradeCallback& callback)
    : socket_(socket.Pass()),
      http_upgrade_callback_(callback),
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
      std::string content_length = ExtractHeader("Content-Length:");
      if (!content_length.empty()) {
        if (!base::StringToInt(content_length, &expected_length)) {
          CheckNetResultOrDie(net::ERR_FAILED);
          return;
        }
      }

      body_pos_ = response_.find("\r\n\r\n");
      if (body_pos_ != std::string::npos) {
        body_pos_ += 4;
        bytes_total = body_pos_ + expected_length;
      }
    }

    if (bytes_total == static_cast<int>(response_.length())) {
      if (!command_callback_.is_null()) {
        command_callback_.Run(net::OK, response_.substr(body_pos_));
      } else {
        http_upgrade_callback_.Run(net::OK,
            ExtractHeader("Sec-WebSocket-Extensions:"), socket_.Pass());
      }
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

  std::string ExtractHeader(const std::string& header) {
    size_t start_pos = response_.find(header);
    if (start_pos == std::string::npos)
      return std::string();

    size_t endline_pos = response_.find("\n", start_pos);
    if (endline_pos == std::string::npos)
      return std::string();

    std::string value = response_.substr(
        start_pos + header.length(), endline_pos - start_pos - header.length());
    base::TrimWhitespace(value, base::TRIM_ALL, &value);
    return value;
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    if (!command_callback_.is_null()) {
      command_callback_.Run(result, std::string());
    } else {
      http_upgrade_callback_.Run(
          result, "", make_scoped_ptr<net::StreamSocket>(nullptr));
    }
    delete this;
    return false;
  }

  scoped_ptr<net::StreamSocket> socket_;
  std::string response_;
  CommandCallback command_callback_;
  HttpUpgradeCallback http_upgrade_callback_;
  size_t body_pos_;
};

class DevicesRequest : public base::RefCountedThreadSafe<DevicesRequest> {
 public:
  typedef AndroidDeviceManager::DeviceInfo DeviceInfo;
  typedef AndroidDeviceManager::DeviceProvider DeviceProvider;
  typedef AndroidDeviceManager::DeviceProviders DeviceProviders;
  typedef AndroidDeviceManager::DeviceDescriptors DeviceDescriptors;
  typedef base::Callback<void(scoped_ptr<DeviceDescriptors>)>
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
        base::Bind(callback_, base::Passed(&descriptors_)));
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
    const std::string& extensions,
    const HttpUpgradeCallback& callback) {
  std::string extensions_with_new_line =
      extensions.empty() ? std::string() : extensions + "\r\n";
  OpenSocket(
      serial,
      socket_name,
      base::Bind(&HttpRequest::HttpUpgradeRequest,
                 base::StringPrintf(kWebSocketUpgradeRequest,
                                    url.c_str(),
                                    extensions_with_new_line.c_str()),
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
  message_loop_proxy_->PostTask(
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
  message_loop_proxy_->PostTask(
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
  message_loop_proxy_->PostTask(
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

void AndroidDeviceManager::Device::HttpUpgrade(
    const std::string& socket_name,
    const std::string& url,
    const std::string& extensions,
    const HttpUpgradeCallback& callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&DeviceProvider::HttpUpgrade,
                 provider_,
                 serial_,
                 socket_name,
                 url,
                 extensions,
                 base::Bind(&PostHttpUpgradeCallback,
                            base::MessageLoopProxy::current(),
                            callback)));
}

AndroidDeviceManager::Device::Device(
    scoped_refptr<base::MessageLoopProxy> device_message_loop,
    scoped_refptr<DeviceProvider> provider,
    const std::string& serial)
    : message_loop_proxy_(device_message_loop),
      provider_(provider),
      serial_(serial),
      weak_factory_(this) {
}

AndroidDeviceManager::Device::~Device() {
  std::set<AndroidWebSocket*> sockets_copy(sockets_);
  for (AndroidWebSocket* socket : sockets_copy)
    socket->OnSocketClosed();

  provider_->AddRef();
  DeviceProvider* raw_ptr = provider_.get();
  provider_ = NULL;
  message_loop_proxy_->PostTask(
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
scoped_ptr<AndroidDeviceManager> AndroidDeviceManager::Create() {
  return make_scoped_ptr(new AndroidDeviceManager());
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
                                   weak_factory_.GetWeakPtr(),
                                   callback));
}

AndroidDeviceManager::AndroidDeviceManager()
    : handler_thread_(HandlerThread::GetInstance()),
      weak_factory_(this) {
}

AndroidDeviceManager::~AndroidDeviceManager() {
  SetDeviceProviders(DeviceProviders());
}

void AndroidDeviceManager::UpdateDevices(
    const DevicesCallback& callback,
    scoped_ptr<DeviceDescriptors> descriptors) {
  Devices response;
  DeviceWeakMap new_devices;
  for (DeviceDescriptors::const_iterator it = descriptors->begin();
       it != descriptors->end();
       ++it) {
    DeviceWeakMap::iterator found = devices_.find(it->serial);
    scoped_refptr<Device> device;
    if (found == devices_.end() || !found->second ||
        found->second->provider_.get() != it->provider.get()) {
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
