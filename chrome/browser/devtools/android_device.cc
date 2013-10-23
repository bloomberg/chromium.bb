// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/android_device.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "chrome/browser/devtools/adb/android_rsa.h"
#include "chrome/browser/devtools/adb/android_usb_device.h"
#include "chrome/browser/devtools/adb_client_socket.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

const char kHostTransportCommand[] = "host:transport:%s|%s";
const char kHostDevicesCommand[] = "host:devices";
const char kLocalAbstractCommand[] = "localabstract:%s";

const int kAdbPort = 5037;
const int kBufferSize = 16 * 1024;

// AdbDeviceImpl --------------------------------------------------------------

class AdbDeviceImpl : public AndroidDevice {
 public:
  AdbDeviceImpl(const std::string& serial, bool is_connected);
  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) OVERRIDE;
  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE;
 private:
  virtual ~AdbDeviceImpl() {}
};

AdbDeviceImpl::AdbDeviceImpl(const std::string& serial, bool is_connected)
    : AndroidDevice(serial, is_connected) {
}

void AdbDeviceImpl::RunCommand(const std::string& command,
                               const CommandCallback& callback) {
  std::string query = base::StringPrintf(kHostTransportCommand,
                                         serial().c_str(), command.c_str());
  AdbClientSocket::AdbQuery(kAdbPort, query, callback);
}

void AdbDeviceImpl::OpenSocket(const std::string& name,
                               const SocketCallback& callback) {
  std::string socket_name =
      base::StringPrintf(kLocalAbstractCommand, name.c_str());
  AdbClientSocket::TransportQuery(kAdbPort, serial(), socket_name, callback);
}

// UsbDeviceImpl --------------------------------------------------------------

class UsbDeviceImpl : public AndroidDevice {
 public:
  explicit UsbDeviceImpl(AndroidUsbDevice* device);
  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) OVERRIDE;
  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE;
 private:
  void OnOpenSocket(const SocketCallback& callback,
                    net::StreamSocket* socket,
                    int result);
  void OpenedForCommand(const CommandCallback& callback,
                        net::StreamSocket* socket,
                        int result);
  void OnRead(net::StreamSocket* socket,
              scoped_refptr<net::IOBuffer> buffer,
              const std::string& data,
              const CommandCallback& callback,
              int result);

  virtual ~UsbDeviceImpl() {}
  scoped_refptr<AndroidUsbDevice> device_;
};


UsbDeviceImpl::UsbDeviceImpl(AndroidUsbDevice* device)
    : AndroidDevice(device->serial(), device->is_connected()),
      device_(device) {
  device_->InitOnCallerThread();
}

void UsbDeviceImpl::RunCommand(const std::string& command,
                               const CommandCallback& callback) {
  net::StreamSocket* socket = device_->CreateSocket(command);
  int result = socket->Connect(base::Bind(&UsbDeviceImpl::OpenedForCommand,
                                          this, callback, socket));
  if (result != net::ERR_IO_PENDING)
    callback.Run(result, std::string());
}

void UsbDeviceImpl::OpenSocket(const std::string& name,
                               const SocketCallback& callback) {
  std::string socket_name =
      base::StringPrintf(kLocalAbstractCommand, name.c_str());
  net::StreamSocket* socket = device_->CreateSocket(socket_name);
  int result = socket->Connect(base::Bind(&UsbDeviceImpl::OnOpenSocket, this,
                                          callback, socket));
  if (result != net::ERR_IO_PENDING)
    callback.Run(result, NULL);
}

void UsbDeviceImpl::OnOpenSocket(const SocketCallback& callback,
                  net::StreamSocket* socket,
                  int result) {
  callback.Run(result, result == net::OK ? socket : NULL);
}

void UsbDeviceImpl::OpenedForCommand(const CommandCallback& callback,
                                     net::StreamSocket* socket,
                                     int result) {
  if (result != net::OK) {
    callback.Run(result, std::string());
    return;
  }
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kBufferSize);
  result = socket->Read(buffer, kBufferSize,
                        base::Bind(&UsbDeviceImpl::OnRead, this,
                                   socket, buffer, std::string(), callback));
  if (result != net::ERR_IO_PENDING)
    OnRead(socket, buffer, std::string(), callback, result);
}

void UsbDeviceImpl::OnRead(net::StreamSocket* socket,
                           scoped_refptr<net::IOBuffer> buffer,
                           const std::string& data,
                           const CommandCallback& callback,
                           int result) {
  if (result <= 0) {
    callback.Run(result, result == 0 ? data : std::string());
    delete socket;
    return;
  }

  std::string new_data = data + std::string(buffer->data(), result);
  result = socket->Read(buffer, kBufferSize,
                        base::Bind(&UsbDeviceImpl::OnRead, this,
                                   socket, buffer, new_data, callback));
  if (result != net::ERR_IO_PENDING)
    OnRead(socket, buffer, new_data, callback, result);
}

// AdbDeviceProvider -------------------------------------------

class AdbDeviceProvider: public AndroidDeviceProvider {
 public:
  virtual void QueryDevices(const QueryDevicesCallback& callback) OVERRIDE;
 private:
  void QueryDevicesOnAdbThread(const QueryDevicesCallback& callback);
  void ReceivedAdbDevices(const QueryDevicesCallback& callback, int result,
                          const std::string& response);

  virtual ~AdbDeviceProvider();
};

AdbDeviceProvider::~AdbDeviceProvider() {
}

void AdbDeviceProvider::QueryDevices(const QueryDevicesCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  adb_thread_->message_loop()->PostTask(
        FROM_HERE, base::Bind(&AdbDeviceProvider::QueryDevicesOnAdbThread,
        this, callback));
}

void AdbDeviceProvider::QueryDevicesOnAdbThread(
    const QueryDevicesCallback& callback) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());

  AdbClientSocket::AdbQuery(
      kAdbPort, kHostDevicesCommand,
      base::Bind(&AdbDeviceProvider::ReceivedAdbDevices, this, callback));
}

void AdbDeviceProvider::ReceivedAdbDevices(const QueryDevicesCallback& callback,
                                           int result_code,
                                           const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());

  AndroidDevices result;

#if defined(DEBUG_DEVTOOLS)
  // For desktop remote debugging.
  result.push_back(new AdbDeviceImpl("", true));
#endif  // defined(DEBUG_DEVTOOLS)

  std::vector<std::string> serials;
  Tokenize(response, "\n", &serials);
  for (size_t i = 0; i < serials.size(); ++i) {
    std::vector<std::string> tokens;
    Tokenize(serials[i], "\t ", &tokens);
    bool offline = tokens.size() > 1 && tokens[1] == "offline";
    result.push_back(new AdbDeviceImpl(tokens[0], !offline));
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&AdbDeviceProvider::RunCallbackOnUIThread,
                          callback, result));
}

// UsbDeviceProvider -------------------------------------------

class UsbDeviceProvider: public AndroidDeviceProvider {
 public:
  explicit UsbDeviceProvider(Profile* profile);

  virtual void QueryDevices(const QueryDevicesCallback& callback) OVERRIDE;
 private:
  virtual ~UsbDeviceProvider();
  void WrapDevicesOnAdbThread(const QueryDevicesCallback& callback,
                              const AndroidUsbDevices& devices);
  void EnumeratedDevices(const QueryDevicesCallback& callback,
                         const AndroidUsbDevices& devices);

  scoped_ptr<crypto::RSAPrivateKey>  rsa_key_;
};

UsbDeviceProvider::UsbDeviceProvider(Profile* profile){
  rsa_key_.reset(AndroidRSAPrivateKey(profile));
}

UsbDeviceProvider::~UsbDeviceProvider() {
}

void UsbDeviceProvider::QueryDevices(const QueryDevicesCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AndroidUsbDevice::Enumerate(rsa_key_.get(),
                              base::Bind(&UsbDeviceProvider::EnumeratedDevices,
                              this, callback));
}

void UsbDeviceProvider::EnumeratedDevices(const QueryDevicesCallback& callback,
                                          const AndroidUsbDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  adb_thread_->message_loop()->PostTask(FROM_HERE,
                          base::Bind(&UsbDeviceProvider::WrapDevicesOnAdbThread,
                          this, callback, devices));
}

void UsbDeviceProvider::WrapDevicesOnAdbThread(
    const QueryDevicesCallback& callback,const AndroidUsbDevices& devices) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  AndroidDevices result;
  for (AndroidUsbDevices::const_iterator it = devices.begin();
      it != devices.end(); ++it)
    result.push_back(new UsbDeviceImpl(*it));

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&UsbDeviceProvider::RunCallbackOnUIThread,
                          callback, result));
}

} // namespace

// AndroidDevice -------------------------------------------

AndroidDevice::AndroidDevice(const std::string& serial, bool is_connected)
    : serial_(serial),
      is_connected_(is_connected) {
}

void AndroidDevice::HttpQuery(
    const std::string& la_name,
    const std::string& request,
    const CommandCallback& callback) {
  OpenSocket(la_name, base::Bind(&AndroidDevice::OnHttpSocketOpened, this,
                                 request, callback));
}

void AndroidDevice::HttpUpgrade(
    const std::string& la_name,
    const std::string& request,
    const SocketCallback& callback) {
  OpenSocket(la_name, base::Bind(&AndroidDevice::OnHttpSocketOpened2, this,
                                 request, callback));
}

AndroidDevice::~AndroidDevice() {
}

void AndroidDevice::OnHttpSocketOpened(
    const std::string& request,
    const CommandCallback& callback,
    int result,
    net::StreamSocket* socket) {
  if (result != net::OK) {
    callback.Run(result, std::string());
    return;
  }
  AdbClientSocket::HttpQuery(socket, request, callback);
}

void AndroidDevice::OnHttpSocketOpened2(
    const std::string& request,
    const SocketCallback& callback,
    int result,
    net::StreamSocket* socket) {
  if (result != net::OK) {
    callback.Run(result, NULL);
    return;
  }
  AdbClientSocket::HttpQuery(socket, request, callback);
}

// AdbCountDevicesCommand -----------------------------------------------------
// TODO(zvorygin): Remove this class.
class AdbCountDevicesCommand : public base::RefCountedThreadSafe<
    AdbCountDevicesCommand, BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<void(int)> Callback;

  AdbCountDevicesCommand(
      scoped_refptr<RefCountedAdbThread> adb_thread,
      const Callback& callback);

 private:
  friend struct BrowserThread::DeleteOnThread<
      BrowserThread::UI>;
  friend class base::DeleteHelper<AdbCountDevicesCommand>;

  virtual ~AdbCountDevicesCommand();
  void RequestAdbDeviceCount();
  void ReceivedAdbDeviceCount(int result, const std::string& response);
  void Respond(int count);

  scoped_refptr<RefCountedAdbThread> adb_thread_;
  Callback callback_;
};

AdbCountDevicesCommand::AdbCountDevicesCommand(
    scoped_refptr<RefCountedAdbThread> adb_thread,
    const Callback& callback)
    : adb_thread_(adb_thread),
      callback_(callback) {
  adb_thread_->message_loop()->PostTask(
      FROM_HERE, base::Bind(&AdbCountDevicesCommand::RequestAdbDeviceCount,
                            this));
}

AdbCountDevicesCommand::~AdbCountDevicesCommand() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void AdbCountDevicesCommand::RequestAdbDeviceCount() {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  AdbClientSocket::AdbQuery(
      kAdbPort, kHostDevicesCommand,
      base::Bind(&AdbCountDevicesCommand::ReceivedAdbDeviceCount, this));
}

void AdbCountDevicesCommand::ReceivedAdbDeviceCount(
    int result,
    const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  std::vector<std::string> serials;
  Tokenize(response, "\n", &serials);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&AdbCountDevicesCommand::Respond, this, serials.size()));
}

void AdbCountDevicesCommand::Respond(int count) {
  callback_.Run(count);
}

// AndroidDeviceProvider ---------------------------------------------------

AndroidDeviceProvider::AndroidDeviceProvider()
  : adb_thread_(RefCountedAdbThread::GetInstance()) {

}

AndroidDeviceProvider::~AndroidDeviceProvider() {
}

// static
void AndroidDeviceProvider::RunCallbackOnUIThread(
    const QueryDevicesCallback& callback,
    const AndroidDevices& result) {
  callback.Run(result);
}

// static
void AndroidDeviceProvider::CountDevices(bool discover_usb_devices,
    const base::Callback<void(int)>& callback) {
  if (discover_usb_devices) {
    AndroidUsbDevice::CountDevices(callback);
    return;
  }

  new AdbCountDevicesCommand(RefCountedAdbThread::GetInstance(), callback);
}

// static
scoped_refptr<AndroidDeviceProvider>
    AndroidDeviceProvider::GetUsbDeviceProvider(Profile* profile) {
  return new UsbDeviceProvider(profile);
}

// static
scoped_refptr<AndroidDeviceProvider>
    AndroidDeviceProvider::GetAdbDeviceProvider() {
  return new AdbDeviceProvider();
}
