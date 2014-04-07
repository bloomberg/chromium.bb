// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/devtools/devtools_adb_bridge.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"

const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";
const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
const char kDumpsysCommand[] = "shell:dumpsys window policy";
const char kListProcessesCommand[] = "shell:ps";
const char kInstalledChromePackagesCommand[] = "shell:pm list packages";
const char kDeviceModel[] = "Nexus 8";

const char kSampleOpenedUnixSocketsWithoutBrowsers[] =
    "Num       RefCount Protocol Flags    Type St Inode Path\n"
    "00000000: 00000004 00000000"
    " 00000000 0002 01  3328 /dev/socket/wpa_wlan0\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01  5394 /dev/socket/vold\n";

const char kSampleDumpsys[] =
    "WINDOW MANAGER POLICY STATE (dumpsys window policy)\r\n"
    "    mStable=(0,50)-(720,1184)\r\n";

const char kSampleListProcesses[] =
    "USER     PID   PPID  VSIZE  RSS     WCHAN    PC         NAME\n"
    "root      1     0     688    508   ffffffff 00000000 S /init\n";

const char kSampleListPackages[] = "package:com.example.app";

static const int kBufferSize = 16*1024;
static const int kAdbPort = 5037;

static const int kAdbMessageHeaderSize = 4;

// This is single connection server which listens on specified port and
// simplifies asynchronous IO.
// To write custom server, extend this class and implement TryProcessData
// method which is invoked everytime data arrives. In case of successful data
// processing(e.g. enough data collected already to parse client reply/request)
// return amount of bytes processed to throw them away from buffer
// To send data, SendData method should be used. This method is non-blocking
// and appends data to be sent to internal buffer.
// Since all calls are non-blocking and no callbacks are given, internal
// overflows may occur in case too heavy traffic.
// In case of heavy traffic performance may suffer because of memcpy calls.
class SingleConnectionServer {
 public:
  SingleConnectionServer(net::IPEndPoint endpoint, int buffer_size);
  virtual ~SingleConnectionServer();

 protected:
  virtual int TryProcessData(const char* data, int size) = 0;
  void SendData(const char* data, int size);

private:
  void AcceptConnection();
  void OnAccepted(int result);

  void ReadData();
  void OnDataRead(int count);

  void WriteData();
  void OnDataWritten(int count);

private:
  int bytes_to_write_;
  scoped_ptr<net::TCPServerSocket> server_socket_;
  scoped_ptr<net::StreamSocket> client_socket_;
  scoped_refptr<net::GrowableIOBuffer> input_buffer_;
  scoped_refptr<net::GrowableIOBuffer> output_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SingleConnectionServer);
};

SingleConnectionServer::SingleConnectionServer(net::IPEndPoint endpoint,
                                               int buffer_size)
    : bytes_to_write_(0) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  input_buffer_ = new net::GrowableIOBuffer();
  input_buffer_->SetCapacity(buffer_size);

  output_buffer_ = new net::GrowableIOBuffer();

  server_socket_.reset(new net::TCPServerSocket(NULL, net::NetLog::Source()));
  server_socket_->Listen(endpoint, 1);

  AcceptConnection();
}

SingleConnectionServer::~SingleConnectionServer() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  server_socket_.reset();

  if (client_socket_) {
    client_socket_->Disconnect();
    client_socket_.reset();
  }
}

void SingleConnectionServer::SendData(const char* data, int size) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if ((output_buffer_->offset() + bytes_to_write_ + size) >
      output_buffer_->capacity()) {
    // If not enough space without relocation
    if (output_buffer_->capacity() < (bytes_to_write_ + size)) {
      // If even buffer is not enough
      int new_size = std::max(output_buffer_->capacity() * 2, size * 2);
      output_buffer_->SetCapacity(new_size);
    }
    memmove(output_buffer_->StartOfBuffer(),
            output_buffer_->data(),
            bytes_to_write_);
    output_buffer_->set_offset(0);
  }

  memcpy(output_buffer_->data() + bytes_to_write_, data, size);
  bytes_to_write_ += size;

  if (bytes_to_write_ == size)
    // If write loop wasn't yet started, then start it
    WriteData();
}

void SingleConnectionServer::AcceptConnection() {
  if (client_socket_) {
    client_socket_->Disconnect();
    client_socket_.reset();
  }

  int accept_result = server_socket_->Accept(&client_socket_,
      base::Bind(&SingleConnectionServer::OnAccepted, base::Unretained(this)));

  if (accept_result != net::ERR_IO_PENDING)
    content::BrowserThread::PostTask(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&SingleConnectionServer::OnAccepted,
                   base::Unretained(this),
                   accept_result));
}

void SingleConnectionServer::OnAccepted(int result) {
  ASSERT_EQ(result, 0);  // Fails if the socket is already in use.
  ReadData();
}

void SingleConnectionServer::ReadData() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  if (input_buffer_->RemainingCapacity() == 0)
    input_buffer_->SetCapacity(input_buffer_->capacity() * 2);

  int read_result = client_socket_->Read(
      input_buffer_.get(),
      input_buffer_->RemainingCapacity(),
      base::Bind(&SingleConnectionServer::OnDataRead, base::Unretained(this)));

  if (read_result != net::ERR_IO_PENDING)
    OnDataRead(read_result);
}

void SingleConnectionServer::OnDataRead(int count) {
  if (count <= 0) {
    AcceptConnection();
    return;
  }

  input_buffer_->set_offset(input_buffer_->offset() + count);

  int bytes_processed;

  do {
    char* data = input_buffer_->StartOfBuffer();
    int data_size = input_buffer_->offset();

    bytes_processed = TryProcessData(data, data_size);

    if (bytes_processed) {
      memmove(data, data + bytes_processed, data_size - bytes_processed);
      input_buffer_->set_offset( data_size - bytes_processed);
    }
  } while (bytes_processed);

  // Posting is needed not to enter deep recursion in case too synchronous IO
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(&SingleConnectionServer::ReadData, base::Unretained(this)));
}

void SingleConnectionServer::WriteData() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  CHECK_GE(output_buffer_->capacity(),
           output_buffer_->offset() + bytes_to_write_) << "Overflow";

  int write_result = client_socket_->Write(
      output_buffer_,
      bytes_to_write_,
      base::Bind(&SingleConnectionServer::OnDataWritten,
                 base::Unretained(this)));
  if (write_result != net::ERR_IO_PENDING)
    OnDataWritten(write_result);
}

void SingleConnectionServer::OnDataWritten(int count) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (count < 0) {
    AcceptConnection();
    return;
  }

  CHECK_GT(count, 0);
  CHECK_GE(output_buffer_->capacity(),
           output_buffer_->offset() + bytes_to_write_) << "Overflow";

  bytes_to_write_ -= count;
  output_buffer_->set_offset(output_buffer_->offset() + count);

  if (bytes_to_write_ != 0)
    // Posting is needed not to enter deep recursion in case too synchronous IO
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
        base::Bind(&SingleConnectionServer::WriteData, base::Unretained(this)));
}


class MockAdbServer: public SingleConnectionServer {
 public:
  MockAdbServer(net::IPEndPoint endpoint, int buffer_size)
      : SingleConnectionServer(endpoint, buffer_size)
  {}

  virtual ~MockAdbServer() {}

 private:
  virtual int TryProcessData(const char* data, int size) OVERRIDE {
    CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    if (size >= kAdbMessageHeaderSize) {
      std::string message_header(data, kAdbMessageHeaderSize);
      int message_size;

      EXPECT_TRUE(base::HexStringToInt(message_header, &message_size));

      if (size >= message_size + kAdbMessageHeaderSize) {
        std::string message_body(data + kAdbMessageHeaderSize, message_size );

        ProcessCommand(message_body);

        return kAdbMessageHeaderSize + message_size;
      }
    }

    return 0;
  }

  void ProcessCommand(const std::string& command) {
    CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    if (command == "host:devices") {
      SendResponse("01498B321301A00A\tdevice\n01498B2B0D01300E\toffline");
    } else if (command == "host:transport:01498B321301A00A") {
      SendResponse("");
    } else if (command == kDeviceModelCommand) {
      SendResponse(kDeviceModel);
    } else if (command == kOpenedUnixSocketsCommand) {
      SendResponse(kSampleOpenedUnixSocketsWithoutBrowsers);
    } else if (command == kDumpsysCommand) {
      SendResponse(kSampleDumpsys);
    } else if (command == kListProcessesCommand) {
      SendResponse(kSampleListProcesses);
    } else if (command == kInstalledChromePackagesCommand) {
      SendResponse(kSampleListPackages);
    } else {
      NOTREACHED() << "Unknown command - " << command;
    }
  }

  void SendResponse(const std::string& response) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    std::stringstream response_stream;
    response_stream << "OKAY";

    int size = response.size();
    if (size > 0) {
      static const char kHexChars[] = "0123456789ABCDEF";
      for (int i = 3; i >= 0; i--)
        response_stream << kHexChars[ (size >> 4*i) & 0x0f ];
      response_stream << response;
    }

    std::string response_data = response_stream.str();
    SendData(response_data.c_str(), response_data.size());
  }
};

class AdbClientSocketTest : public InProcessBrowserTest,
                            public DevToolsAdbBridge::Listener {

public:
  void StartTest() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&AdbClientSocketTest::StartMockAdbServer,
                   base::Unretained(this)),
        base::Bind(&AdbClientSocketTest::AddListener,
                   base::Unretained(this)));
  }

  virtual void RemoteDevicesChanged(DevToolsAdbBridge::RemoteDevices* devices)
      OVERRIDE {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    adb_bridge_->RemoveListener(this);
    devices_ = *devices;
    EndTest();
  }

  void CheckDevices() {
    ASSERT_EQ(2U, devices_.size());

    scoped_refptr<DevToolsAdbBridge::RemoteDevice> online_device_;
    scoped_refptr<DevToolsAdbBridge::RemoteDevice> offline_device_;

    for (DevToolsAdbBridge::RemoteDevices::const_iterator it =
        devices_.begin(); it != devices_.end(); ++it) {
      if ((*it)->serial() == "01498B321301A00A")
        online_device_ = *it;
      else if ((*it)->serial() == "01498B2B0D01300E")
        offline_device_ = *it;
    }

    ASSERT_EQ(online_device_->serial(), "01498B321301A00A");
    ASSERT_TRUE(online_device_->is_connected());
    ASSERT_FALSE(offline_device_->is_connected());

    ASSERT_EQ(online_device_->model(), kDeviceModel);
    ASSERT_EQ(online_device_->browsers().size(), 0U);
    ASSERT_EQ(online_device_->screen_size(), gfx::Size(720, 1184));
  }

private:
  void EndTest() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    adb_bridge_ = NULL;

    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&AdbClientSocketTest::StopMockAdbServer,
                   base::Unretained(this)),
        base::Bind(&AdbClientSocketTest::StopMessageLoop,
                   base::Unretained(this)));
  }

  void StartMockAdbServer() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    net::IPAddressNumber address;
    net::ParseIPLiteralToNumber("127.0.0.1", &address);
    net::IPEndPoint endpoint(address, kAdbPort);

    adb_server_.reset(new MockAdbServer(endpoint, kBufferSize));
  }

  void StopMockAdbServer() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    adb_server_.reset();
  }

  void StopMessageLoop() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    runner->Quit();
  }

  void AddListener() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    adb_bridge_ = DevToolsAdbBridge::Factory::GetForProfile(
        browser()->profile());

    DevToolsAdbBridge::DeviceProviders device_providers;
    device_providers.push_back(AndroidDeviceProvider::GetAdbDeviceProvider());

    adb_bridge_->set_device_providers(device_providers);
    adb_bridge_->AddListener(this);
  }

public:
  scoped_refptr<content::MessageLoopRunner> runner;

private:
  scoped_ptr<MockAdbServer> adb_server_;
  scoped_refptr<DevToolsAdbBridge> adb_bridge_;
  DevToolsAdbBridge::RemoteDevices devices_;
};

IN_PROC_BROWSER_TEST_F(AdbClientSocketTest, TestAdbClientSocket) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  runner = new content::MessageLoopRunner;

  StartTest();

  runner->Run();

  CheckDevices();
}
