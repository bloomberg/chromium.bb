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

namespace {

const int kBufferSize = 16 * 1024;

static const char kWebSocketUpgradeRequest[] = "GET %s HTTP/1.1\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";

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
    : socket_(socket),
      command_callback_(callback),
      body_pos_(0) {
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

} // namespace

AndroidDeviceManager::BrowserInfo::BrowserInfo()
    : type(kTypeOther) {
}

AndroidDeviceManager::DeviceInfo::DeviceInfo() {
}

AndroidDeviceManager::DeviceInfo::~DeviceInfo() {
}

AndroidDeviceManager::Device::Device(const std::string& serial,
                                     bool is_connected)
    : serial_(serial),
      is_connected_(is_connected) {
}

AndroidDeviceManager::Device::~Device() {
}

AndroidDeviceManager::DeviceProvider::DeviceProvider() {
}

AndroidDeviceManager::DeviceProvider::~DeviceProvider() {
}

// static
scoped_refptr<AndroidDeviceManager> AndroidDeviceManager::Create() {
  return new AndroidDeviceManager();
}

void AndroidDeviceManager::QueryDevices(
    const DeviceProviders& providers,
    const QueryDevicesCallback& callback) {
  DCHECK(CalledOnValidThread());
  stopped_ = false;
  Devices empty;
  QueryNextProvider(callback, providers, empty, empty);
}

void AndroidDeviceManager::Stop() {
  DCHECK(CalledOnValidThread());
  stopped_ = true;
  devices_.clear();
}

bool AndroidDeviceManager::IsConnected(const std::string& serial) {
  DCHECK(CalledOnValidThread());
  Device* device = FindDevice(serial);
  return device && device->is_connected();
}

void AndroidDeviceManager::QueryDeviceInfo(const std::string& serial,
                                           const DeviceInfoCallback& callback) {
  DCHECK(CalledOnValidThread());
  Device* device = FindDevice(serial);
  if (device)
    device->QueryDeviceInfo(callback);
  else
    callback.Run(DeviceInfo());
}

void AndroidDeviceManager::OpenSocket(
    const std::string& serial,
    const std::string& socket_name,
    const SocketCallback& callback) {
  DCHECK(CalledOnValidThread());
  Device* device = FindDevice(serial);
  if (device)
    device->OpenSocket(socket_name, callback);
  else
    callback.Run(net::ERR_CONNECTION_FAILED, NULL);
}

void AndroidDeviceManager::HttpQuery(
    const std::string& serial,
    const std::string& socket_name,
    const std::string& request,
    const CommandCallback& callback) {
  DCHECK(CalledOnValidThread());
  Device* device = FindDevice(serial);
  if (device)
    device->OpenSocket(socket_name,
        base::Bind(&HttpRequest::CommandRequest, request, callback));
  else
    callback.Run(net::ERR_CONNECTION_FAILED, std::string());
}

void AndroidDeviceManager::HttpUpgrade(
    const std::string& serial,
    const std::string& socket_name,
    const std::string& url,
    const SocketCallback& callback) {
  DCHECK(CalledOnValidThread());
  Device* device = FindDevice(serial);
  if (device) {
    device->OpenSocket(
        socket_name,
        base::Bind(&HttpRequest::SocketRequest,
                   base::StringPrintf(kWebSocketUpgradeRequest, url.c_str()),
                   callback));
  } else {
    callback.Run(net::ERR_CONNECTION_FAILED, NULL);
  }
}

AndroidDeviceManager::AndroidDeviceManager()
    : stopped_(false) {
}

AndroidDeviceManager::~AndroidDeviceManager() {
}

void AndroidDeviceManager::QueryNextProvider(
    const QueryDevicesCallback& callback,
    const DeviceProviders& providers,
    const Devices& total_devices,
    const Devices& new_devices) {
  DCHECK(CalledOnValidThread());

  if (stopped_)
    return;

  Devices more_devices(total_devices);
  more_devices.insert(
      more_devices.end(), new_devices.begin(), new_devices.end());

  if (providers.empty()) {
    std::vector<std::string> serials;
    devices_.clear();
    for (Devices::const_iterator it = more_devices.begin();
        it != more_devices.end(); ++it) {
      devices_[(*it)->serial()] = *it;
      serials.push_back((*it)->serial());
    }
    callback.Run(serials);
    return;
  }

  scoped_refptr<DeviceProvider> current_provider = providers.back();
  DeviceProviders less_providers = providers;
  less_providers.pop_back();
  current_provider->QueryDevices(
      base::Bind(&AndroidDeviceManager::QueryNextProvider,
                 this, callback, less_providers, more_devices));
}

AndroidDeviceManager::Device*
AndroidDeviceManager::FindDevice(const std::string& serial) {
  DCHECK(CalledOnValidThread());
  DeviceMap::const_iterator it = devices_.find(serial);
  if (it == devices_.end())
    return NULL;
  return (*it).second.get();
}
