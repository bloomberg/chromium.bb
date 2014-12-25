// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/device/usb/android_usb_device.h"
#include "chrome/browser/devtools/device/usb/usb_device_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using device::UsbConfigDescriptor;
using device::UsbDevice;
using device::UsbDeviceHandle;
using device::UsbEndpointDescriptor;
using device::UsbEndpointDirection;
using device::UsbInterfaceDescriptor;
using device::UsbService;
using device::UsbSynchronizationType;
using device::UsbTransferCallback;
using device::UsbTransferType;
using device::UsbUsageType;

namespace {

struct AndroidTraits {
  static const int kClass = 0xff;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x1;
  static const bool kBreaks = false;
};

struct NonAndroidTraits {
  static const int kClass = 0xf0;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x2;
  static const bool kBreaks = false;
};

struct BreakingAndroidTraits {
  static const int kClass = 0xff;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x1;
  static const bool kBreaks = true;
};

const uint32 kMaxPayload = 4096;
const uint32 kVersion = 0x01000000;

const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";
const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
const char kDumpsysCommand[] = "shell:dumpsys window policy";
const char kListProcessesCommand[] = "shell:ps";
const char kInstalledChromePackagesCommand[] = "shell:pm list packages";
const char kOpenSocketCommand[] = "localabstract:chrome_devtools_remote";
const char kDeviceManufacturer[] = "Test Manufacturer";
const char kDeviceModel[] = "Nexus 5";
const char kDeviceSerial[] = "Sample serial";

const char kHttpVersionRequest[] = "GET /json/version HTTP/1.1\r\n\r\n";
const char kHttpPagesRequest[] = "GET /json HTTP/1.1\r\n\r\n";

const char kSampleOpenedUnixSockets[] =
    "Num       RefCount Protocol Flags    Type St Inode Path\r\n"
    "00000000: 00000004 00000000"
    " 00000000 0002 01  3328 /dev/socket/wpa_wlan0\r\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01  5394 /dev/socket/vold\r\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 1095814 @chrome_devtools_remote\r\n";

const char kSampleListProcesses[] =
    "USER   PID  PPID VSIZE  RSS    WCHAN    PC         NAME\r\n"
    "root   1    0    688    508    ffffffff 00000000 S /init\r\n"
    "u0_a75 2425 123  933736 193024 ffffffff 00000000 S com.sample.feed\r\n"
    "nfc    741  123  706448 26316  ffffffff 00000000 S com.android.nfc\r\n"
    "u0_a76 1001 124  111111 222222 ffffffff 00000000 S com.android.chrome\r\n"
    "u0_a78 1003 126  111111 222222 ffffffff 00000000 S com.noprocess.app\r\n";

const char kSampleListPackages[] =
    "package:com.sample.feed\r\n"
    "package:com.android.nfc\r\n"
    "package:com.android.chrome\r\n"
    "package:com.chrome.beta\r\n"
    "package:com.google.android.apps.chrome\r\n";

const char kSampleDumpsys[] =
    "WINDOW MANAGER POLICY STATE (dumpsys window policy)\r\n"
    "    mSafeMode=false mSystemReady=true mSystemBooted=true\r\n"
    "    mStable=(0,50)-(720,1184)\r\n"  // Only mStable parameter is parsed
    "    mForceStatusBar=false mForceStatusBarFromKeyguard=false\r\n";

const char kSampleBrowserVersion[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length:312\r\n"
    "Content-Type:application/json; charset=UTF-8\r\n\r\n"
    "{\n"
    "   \"Android-Package\": \"com.android.chrome\",\n"
    "   \"Browser\": \"Chrome/39.0.2171.93\",\n"
    "   \"Protocol-Version\": \"1.1\",\n"
    "   \"User-Agent\": \"Mozilla/5.0 (Linux; Android 5.0.1; Nexus 5 "
    "Build/LRX22C) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.93 "
    "Mobile Safari/537.36\",\n"
    "   \"WebKit-Version\": \"537.36 (@185626)\"\n"
    "}\n";

const char kSampleBrowserPages[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length:397\r\n"
    "Content-Type:application/json; charset=UTF-8\r\n\r\n"
    "[ {\n"
    "   \"description\": \"\",\n"
    "   \"devtoolsFrontendUrl\": \"http://chrome-devtools-frontend.appspot.com"
    "/serve_rev/@185626/devtools.html?ws=/devtools/page/377\",\n"
    "   \"faviconUrl\": "
    "\"http://www.chromium.org/_/rsrc/1354323194313/favicon.ico\",\n"
    "   \"id\": \"377\",\n"
    "   \"title\": \"The Chromium Projects\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"http://www.chromium.org/\",\n"
    "   \"webSocketDebuggerUrl\": \"ws:///devtools/page/377\"\n"
    "} ]\n";

const char* GetMockShellResponse(const std::string& command) {
  if (command == kDeviceModelCommand) {
    return kDeviceModel;
  } else if (command == kOpenedUnixSocketsCommand) {
    return kSampleOpenedUnixSockets;
  } else if (command == kDumpsysCommand) {
    return kSampleDumpsys;
  } else if (command == kListProcessesCommand) {
    return kSampleListProcesses;
  } else if (command == kInstalledChromePackagesCommand) {
    return kSampleListPackages;
  }
  NOTREACHED();
  return "";
}

const char* GetMockHttpResponse(const std::string& request) {
  if (request == kHttpVersionRequest) {
    return kSampleBrowserVersion;
  } else if (request == kHttpPagesRequest) {
    return kSampleBrowserPages;
  }
  NOTREACHED();
  return "";
}

template <class T>
class MockUsbDevice;

template <class T>
class MockUsbDeviceHandle : public UsbDeviceHandle {
 public:
  explicit MockUsbDeviceHandle(MockUsbDevice<T>* device)
      : device_(device),
        remaining_body_length_(0),
        last_local_socket_(0),
        broken_(false) {
  }

  virtual scoped_refptr<UsbDevice> GetDevice() const override {
    return device_;
  }

  virtual void Close() override { device_ = nullptr; }

  bool ClaimInterface(int interface_number) override {
    if (device_->claimed_interfaces_.find(interface_number) !=
        device_->claimed_interfaces_.end())
      return false;

    device_->claimed_interfaces_.insert(interface_number);
    return true;
  }

  bool ReleaseInterface(int interface_number) override {
    if (device_->claimed_interfaces_.find(interface_number) ==
        device_->claimed_interfaces_.end())
      return false;

    device_->claimed_interfaces_.erase(interface_number);
    return true;
  }

  virtual bool SetInterfaceAlternateSetting(int interface_number,
                                            int alternate_setting) override {
    return true;
  }

  virtual bool ResetDevice() override { return true; }
  bool GetStringDescriptor(uint8_t string_id,
                           base::string16* content) override {
    return false;
  }

  // Async IO. Can be called on any thread.
  virtual void ControlTransfer(UsbEndpointDirection direction,
                               TransferRequestType request_type,
                               TransferRecipient recipient,
                               uint8 request,
                               uint16 value,
                               uint16 index,
                               net::IOBuffer* buffer,
                               size_t length,
                               unsigned int timeout,
                               const UsbTransferCallback& callback) override {}

  virtual void BulkTransfer(UsbEndpointDirection direction,
                            uint8 endpoint,
                            net::IOBuffer* buffer,
                            size_t length,
                            unsigned int timeout,
                            const UsbTransferCallback& callback) override {
    if (direction == device::USB_DIRECTION_OUTBOUND) {
      if (remaining_body_length_ == 0) {
        std::vector<uint32> header(6);
        memcpy(&header[0], buffer->data(), length);
        current_message_.reset(
            new AdbMessage(header[0], header[1], header[2], std::string()));
        remaining_body_length_ = header[3];
        uint32 magic = header[5];
        if ((current_message_->command ^ 0xffffffff) != magic) {
          DCHECK(false) << "Header checksum error";
          return;
        }
      } else {
        DCHECK(current_message_.get());
        current_message_->body += std::string(buffer->data(), length);
        remaining_body_length_ -= length;
      }

      if (remaining_body_length_ == 0) {
        ProcessIncoming();
      }

      device::UsbTransferStatus status =
          broken_ ? device::USB_TRANSFER_ERROR : device::USB_TRANSFER_COMPLETED;
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback, status, scoped_refptr<net::IOBuffer>(), 0));
      ProcessQueries();
    } else if (direction == device::USB_DIRECTION_INBOUND) {
      queries_.push(Query(callback, make_scoped_refptr(buffer), length));
      ProcessQueries();
    }
  }

  template <class D>
  void append(D data) {
    std::copy(reinterpret_cast<char*>(&data),
              (reinterpret_cast<char*>(&data)) + sizeof(D),
              std::back_inserter(output_buffer_));
  }

  // Copied from AndroidUsbDevice::Checksum
  uint32 Checksum(const std::string& data) {
    unsigned char* x = (unsigned char*)data.data();
    int count = data.length();
    uint32 sum = 0;
    while (count-- > 0)
      sum += *x++;
    return sum;
  }

  void ProcessIncoming() {
    DCHECK(current_message_.get());
    switch (current_message_->command) {
      case AdbMessage::kCommandCNXN: {
        WriteResponse(make_scoped_ptr(
            new AdbMessage(AdbMessage::kCommandCNXN,
                           kVersion,
                           kMaxPayload,
                           "device::ro.product.name=SampleProduct;ro."
                           "product.model=SampleModel;ro.product."
                           "device=SampleDevice;")));
        break;
      }
      case AdbMessage::kCommandWRTE: {
        if (T::kBreaks) {
          broken_ = true;
          return;
        }
        auto it = local_sockets_.find(current_message_->arg0);
        if (it == local_sockets_.end())
          return;

        DCHECK((int)current_message_->arg1 == it->second);
        WriteResponse(make_scoped_ptr(new AdbMessage(AdbMessage::kCommandOKAY,
                                                     current_message_->arg1,
                                                     current_message_->arg0,
                                                     std::string())));
        WriteResponse(make_scoped_ptr(
            new AdbMessage(AdbMessage::kCommandWRTE,
                           current_message_->arg1,
                           current_message_->arg0,
                           GetMockHttpResponse(current_message_->body))));
        WriteResponse(make_scoped_ptr(new AdbMessage(AdbMessage::kCommandCLSE,
                                                     0,
                                                     current_message_->arg0,
                                                     std::string())));
        break;
      }
      case AdbMessage::kCommandOPEN: {
        DCHECK(current_message_->arg1 == 0);
        DCHECK(current_message_->arg0 != 0);
        std::string response;
        if (current_message_->body.find("shell:") == (size_t)0) {
          WriteResponse(make_scoped_ptr(new AdbMessage(AdbMessage::kCommandOKAY,
                                                       ++last_local_socket_,
                                                       current_message_->arg0,
                                                       std::string())));
          WriteResponse(make_scoped_ptr(
              new AdbMessage(AdbMessage::kCommandWRTE,
                             last_local_socket_,
                             current_message_->arg0,
                             GetMockShellResponse(current_message_->body.substr(
                                 0, current_message_->body.size() - 1)))));
          WriteResponse(make_scoped_ptr(new AdbMessage(AdbMessage::kCommandCLSE,
                                                       0,
                                                       current_message_->arg0,
                                                       std::string())));
        }
        if (current_message_->body.find(kOpenSocketCommand) == (size_t)0) {
          local_sockets_[current_message_->arg0] = ++last_local_socket_;
          WriteResponse(make_scoped_ptr(new AdbMessage(AdbMessage::kCommandOKAY,
                                                       last_local_socket_,
                                                       current_message_->arg0,
                                                       std::string())));
        }
        return;
      }
      default: {
        return;
      }
    }
    ProcessQueries();
  }

  void WriteResponse(scoped_ptr<AdbMessage> response) {
    append(response->command);
    append(response->arg0);
    append(response->arg1);
    bool add_zero = response->body.length() &&
                    (response->command != AdbMessage::kCommandWRTE);
    append(static_cast<uint32>(response->body.length() + (add_zero ? 1 : 0)));
    append(Checksum(response->body));
    append(response->command ^ 0xffffffff);
    std::copy(response->body.begin(),
              response->body.end(),
              std::back_inserter(output_buffer_));
    if (add_zero) {
      output_buffer_.push_back(0);
    }
    ProcessQueries();
  }

  void ProcessQueries() {
    if (!queries_.size())
      return;
    Query query = queries_.front();
    if (broken_) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(query.callback,
                     device::USB_TRANSFER_ERROR,
                     nullptr,
                     0));
    }

    if (query.size > output_buffer_.size())
      return;

    queries_.pop();
    std::copy(output_buffer_.begin(),
              output_buffer_.begin() + query.size,
              query.buffer->data());
    output_buffer_.erase(output_buffer_.begin(),
                         output_buffer_.begin() + query.size);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(query.callback,
                   device::USB_TRANSFER_COMPLETED,
                   query.buffer,
                   query.size));

  }

  virtual void InterruptTransfer(UsbEndpointDirection direction,
                                 uint8 endpoint,
                                 net::IOBuffer* buffer,
                                 size_t length,
                                 unsigned int timeout,
                                 const UsbTransferCallback& callback) override {
  }

  virtual void IsochronousTransfer(
      UsbEndpointDirection direction,
      uint8 endpoint,
      net::IOBuffer* buffer,
      size_t length,
      unsigned int packets,
      unsigned int packet_length,
      unsigned int timeout,
      const UsbTransferCallback& callback) override {}

 protected:
  virtual ~MockUsbDeviceHandle() {}

  struct Query {
    UsbTransferCallback callback;
    scoped_refptr<net::IOBuffer> buffer;
    size_t size;

    Query(UsbTransferCallback callback,
          scoped_refptr<net::IOBuffer> buffer,
          int size)
        : callback(callback), buffer(buffer), size(size) {};
  };

  scoped_refptr<MockUsbDevice<T> > device_;
  uint32 remaining_body_length_;
  scoped_ptr<AdbMessage> current_message_;
  std::vector<char> output_buffer_;
  std::queue<Query> queries_;
  std::map<int, int> local_sockets_;
  int last_local_socket_;
  bool broken_;
};

template <class T>
class MockUsbDevice : public UsbDevice {
 public:
  MockUsbDevice() : UsbDevice(0, 0, 0) {
    UsbEndpointDescriptor bulk_in;
    bulk_in.address = 0x81;
    bulk_in.direction = device::USB_DIRECTION_INBOUND;
    bulk_in.maximum_packet_size = 512;
    bulk_in.transfer_type = device::USB_TRANSFER_BULK;

    UsbEndpointDescriptor bulk_out;
    bulk_out.address = 0x01;
    bulk_out.direction = device::USB_DIRECTION_OUTBOUND;
    bulk_out.maximum_packet_size = 512;
    bulk_out.transfer_type = device::USB_TRANSFER_BULK;

    UsbInterfaceDescriptor interface_desc;
    interface_desc.interface_number = 0;
    interface_desc.alternate_setting = 0;
    interface_desc.interface_class = T::kClass;
    interface_desc.interface_subclass = T::kSubclass;
    interface_desc.interface_protocol = T::kProtocol;
    interface_desc.endpoints.push_back(bulk_in);
    interface_desc.endpoints.push_back(bulk_out);

    config_desc_.interfaces.push_back(interface_desc);
  }

  virtual scoped_refptr<UsbDeviceHandle> Open() override {
    return new MockUsbDeviceHandle<T>(this);
  }

  virtual const UsbConfigDescriptor& GetConfiguration() override {
    return config_desc_;
  }

  virtual bool GetManufacturer(base::string16* manufacturer) override {
    *manufacturer = base::UTF8ToUTF16(kDeviceManufacturer);
    return true;
  }

  virtual bool GetProduct(base::string16* product) override {
    *product = base::UTF8ToUTF16(kDeviceModel);
    return true;
  }

  virtual bool GetSerialNumber(base::string16* serial) override {
    *serial = base::UTF8ToUTF16(kDeviceSerial);
    return true;
  }

  virtual bool Close(scoped_refptr<UsbDeviceHandle> handle) override {
    return true;
  }

#if defined(OS_CHROMEOS)
  // On ChromeOS, if an interface of a claimed device is not claimed, the
  // permission broker can change the owner of the device so that the unclaimed
  // interfaces can be used. If this argument is missing, permission broker will
  // not be used and this method fails if the device is claimed.
  virtual void RequestUsbAccess(
      int interface_id,
      const base::Callback<void(bool success)>& callback) override {
    callback.Run(true);
  }
#endif  // OS_CHROMEOS

  std::set<int> claimed_interfaces_;

 protected:
  virtual ~MockUsbDevice() {}

 private:
  UsbConfigDescriptor config_desc_;
};

class MockUsbService : public UsbService {
 public:
  MockUsbService() {
    devices_.push_back(new MockUsbDevice<AndroidTraits>());
  }

  ~MockUsbService() override {}

  scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  void GetDevices(std::vector<scoped_refptr<UsbDevice>>* devices) override {
    STLClearObject(devices);
    std::copy(devices_.begin(), devices_.end(), back_inserter(*devices));
  }

  std::vector<scoped_refptr<UsbDevice> > devices_;
};

class MockBreakingUsbService : public UsbService {
 public:
  MockBreakingUsbService() {}
  ~MockBreakingUsbService() {}

  scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  void GetDevices(std::vector<scoped_refptr<UsbDevice>>* devices) override {
    STLClearObject(devices);
    devices->push_back(new MockUsbDevice<BreakingAndroidTraits>());
  }
};

class MockUsbServiceForCheckingTraits : public UsbService {
 public:
  MockUsbServiceForCheckingTraits() : step_(0) {}

  ~MockUsbServiceForCheckingTraits() override {}

  scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

  void GetDevices(std::vector<scoped_refptr<UsbDevice>>* devices) override {
    STLClearObject(devices);
    // This switch should be kept in sync with
    // AndroidUsbBrowserTest::DeviceCountChanged.
    switch (step_) {
      case 0:
        // No devices.
        break;
      case 1:
        // Android device.
        devices->push_back(new MockUsbDevice<AndroidTraits>());
        break;
      case 2:
        // Android and non-android device.
        devices->push_back(new MockUsbDevice<AndroidTraits>());
        devices->push_back(new MockUsbDevice<NonAndroidTraits>());
        break;
      case 3:
        // Non-android device.
        devices->push_back(new MockUsbDevice<NonAndroidTraits>());
        break;
    }
    step_++;
  }

 private:
  int step_;
};

class DevToolsAndroidBridgeWarmUp
    : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  DevToolsAndroidBridgeWarmUp(base::Closure closure,
                              DevToolsAndroidBridge* adb_bridge)
      : closure_(closure), adb_bridge_(adb_bridge) {}

  void DeviceCountChanged(int count) override {
    adb_bridge_->RemoveDeviceCountListener(this);
    closure_.Run();
  }

  base::Closure closure_;
  DevToolsAndroidBridge* adb_bridge_;
};

class AndroidUsbDiscoveryTest : public InProcessBrowserTest {
 protected:
  AndroidUsbDiscoveryTest()
      : scheduler_invoked_(0) {
  }
  void SetUpOnMainThread() override {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;

    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&AndroidUsbDiscoveryTest::SetUpService, this),
        runner->QuitClosure());
    runner->Run();

    adb_bridge_ =
        DevToolsAndroidBridge::Factory::GetForProfile(browser()->profile());
    DCHECK(adb_bridge_);
    adb_bridge_->set_task_scheduler_for_test(base::Bind(
        &AndroidUsbDiscoveryTest::ScheduleDeviceCountRequest, this));

    scoped_refptr<UsbDeviceProvider> provider =
        new UsbDeviceProvider(browser()->profile());

    AndroidDeviceManager::DeviceProviders providers;
    providers.push_back(provider);
    adb_bridge_->set_device_providers_for_test(providers);
    runner_ = new content::MessageLoopRunner;
  }

  void ScheduleDeviceCountRequest(const base::Closure& request) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    scheduler_invoked_++;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, request);
  }

  virtual void SetUpService() {
    UsbService::SetInstanceForTest(new MockUsbService());
  }

  void TearDownOnMainThread() override {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    UsbService* service = nullptr;
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&UsbService::SetInstanceForTest, service),
        runner->QuitClosure());
    runner->Run();
  }

  scoped_refptr<content::MessageLoopRunner> runner_;
  DevToolsAndroidBridge* adb_bridge_;
  int scheduler_invoked_;
};

class AndroidUsbCountTest : public AndroidUsbDiscoveryTest {
 protected:
  void SetUpOnMainThread() override {
    AndroidUsbDiscoveryTest::SetUpOnMainThread();
    DevToolsAndroidBridgeWarmUp warmup(runner_->QuitClosure(), adb_bridge_);
    adb_bridge_->AddDeviceCountListener(&warmup);
    runner_->Run();
    runner_ = new content::MessageLoopRunner;
  }
};

class AndroidUsbTraitsTest : public AndroidUsbDiscoveryTest {
 protected:
  void SetUpService() override {
    UsbService::SetInstanceForTest(new MockUsbServiceForCheckingTraits());
  }
};

class AndroidBreakingUsbTest : public AndroidUsbDiscoveryTest {
 protected:
  void SetUpService() override {
    UsbService::SetInstanceForTest(new MockBreakingUsbService());
  }
};

class MockListListener : public DevToolsAndroidBridge::DeviceListListener {
 public:
  MockListListener(DevToolsAndroidBridge* adb_bridge,
                   const base::Closure& callback)
      : adb_bridge_(adb_bridge),
        callback_(callback) {
  }

  void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) override {
    if (devices.size() > 0) {
      if (devices[0]->is_connected()) {
        ASSERT_EQ(kDeviceModel, devices[0]->model());
        ASSERT_EQ(kDeviceSerial, devices[0]->serial());
        adb_bridge_->RemoveDeviceListListener(this);
        callback_.Run();
      }
    }
  }

  DevToolsAndroidBridge* adb_bridge_;
  base::Closure callback_;
};

class MockCountListener : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  explicit MockCountListener(DevToolsAndroidBridge* adb_bridge)
      : adb_bridge_(adb_bridge),
        reposts_left_(10),
        invoked_(0) {
  }

  void DeviceCountChanged(int count) override {
    ++invoked_;
    adb_bridge_->RemoveDeviceCountListener(this);
    Shutdown();
  }

  void Shutdown() {
    ShutdownOnUIThread();
  };

  void ShutdownOnUIThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (reposts_left_-- == 0) {
      base::MessageLoop::current()->Quit();
    } else {
      BrowserThread::PostTask(
          BrowserThread::FILE,
          FROM_HERE,
          base::Bind(&MockCountListener::ShutdownOnFileThread,
                     base::Unretained(this)));
    }
  }

  void ShutdownOnFileThread() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&MockCountListener::ShutdownOnUIThread,
                                       base::Unretained(this)));
  }

  DevToolsAndroidBridge* adb_bridge_;
  int reposts_left_;
  int invoked_;
};

class MockCountListenerWithReAdd : public MockCountListener {
 public:
  explicit MockCountListenerWithReAdd(
      DevToolsAndroidBridge* adb_bridge)
      : MockCountListener(adb_bridge),
        readd_count_(2) {
  }

  void DeviceCountChanged(int count) override {
    ++invoked_;
    adb_bridge_->RemoveDeviceCountListener(this);
    if (readd_count_ > 0) {
      readd_count_--;
      adb_bridge_->AddDeviceCountListener(this);
      adb_bridge_->RemoveDeviceCountListener(this);
      adb_bridge_->AddDeviceCountListener(this);
    } else {
      Shutdown();
    }
  }

  int readd_count_;
};

class MockCountListenerWithReAddWhileQueued : public MockCountListener {
 public:
  MockCountListenerWithReAddWhileQueued(
      DevToolsAndroidBridge* adb_bridge)
      : MockCountListener(adb_bridge),
        readded_(false) {
  }

  void DeviceCountChanged(int count) override {
    ++invoked_;
    if (!readded_) {
      readded_ = true;
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&MockCountListenerWithReAddWhileQueued::ReAdd,
                     base::Unretained(this)));
    } else {
      adb_bridge_->RemoveDeviceCountListener(this);
      Shutdown();
    }
  }

  void ReAdd() {
    adb_bridge_->RemoveDeviceCountListener(this);
    adb_bridge_->AddDeviceCountListener(this);
  }

  bool readded_;
};

class MockCountListenerForCheckingTraits : public MockCountListener {
 public:
  MockCountListenerForCheckingTraits(
      DevToolsAndroidBridge* adb_bridge)
      : MockCountListener(adb_bridge),
        step_(0) {
  }
  void DeviceCountChanged(int count) override {
    switch (step_) {
      case 0:
        // Check for 0 devices when no devices present.
        EXPECT_EQ(0, count);
        break;
      case 1:
        // Check for 1 device when only android device present.
        EXPECT_EQ(1, count);
        break;
      case 2:
        // Check for 1 device when android and non-android devices present.
        EXPECT_EQ(1, count);
        break;
      case 3:
        // Check for 0 devices when only non-android devices present.
        EXPECT_EQ(0, count);
        adb_bridge_->RemoveDeviceCountListener(this);
        Shutdown();
        break;
      default:
        EXPECT_TRUE(false) << "Unknown step " << step_;
    }
    step_++;
  }

  int step_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AndroidUsbDiscoveryTest, TestDeviceDiscovery) {
  MockListListener listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceListListener(&listener);
  runner_->Run();
}

IN_PROC_BROWSER_TEST_F(AndroidBreakingUsbTest, TestDeviceBreaking) {
  MockListListener listener(adb_bridge_, runner_->QuitClosure());
  adb_bridge_->AddDeviceListListener(&listener);
  runner_->Run();
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveInCallback) {
  MockCountListener listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(1, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddInCallback) {
  MockCountListenerWithReAdd listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(3, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddOnStart) {
  MockCountListener listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  adb_bridge_->RemoveDeviceCountListener(&listener);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(1, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(AndroidUsbCountTest,
                       TestNoMultipleCallsRemoveAddWhileQueued) {
  MockCountListenerWithReAddWhileQueued listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
  EXPECT_EQ(2, listener.invoked_);
  EXPECT_EQ(listener.invoked_ - 1, scheduler_invoked_);
}

IN_PROC_BROWSER_TEST_F(AndroidUsbTraitsTest, TestDeviceCounting) {
  MockCountListenerForCheckingTraits listener(adb_bridge_);
  adb_bridge_->AddDeviceCountListener(&listener);
  runner_->Run();
}
