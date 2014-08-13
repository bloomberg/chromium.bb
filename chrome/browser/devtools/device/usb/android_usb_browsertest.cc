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
#include "components/usb_service/usb_device.h"
#include "components/usb_service/usb_device_handle.h"
#include "components/usb_service/usb_interface.h"
#include "components/usb_service/usb_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using namespace usb_service;

namespace {

struct AndroidTraits {
  static const int kClass = 0xff;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x1;
};

struct NonAndroidTraits {
  static const int kClass = 0xf0;
  static const int kSubclass = 0x42;
  static const int kProtocol = 0x2;
};

const uint32 kMaxPayload = 4096;
const uint32 kVersion = 0x01000000;

const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";
const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
const char kDumpsysCommand[] = "shell:dumpsys window policy";
const char kListProcessesCommand[] = "shell:ps";
const char kInstalledChromePackagesCommand[] = "shell:pm list packages";
const char kDeviceModel[] = "Nexus 5";
const char kDeviceSerial[] = "Sample serial";

const char kSampleOpenedUnixSockets[] =
    "Num       RefCount Protocol Flags    Type St Inode Path\n"
    "00000000: 00000004 00000000"
    " 00000000 0002 01  3328 /dev/socket/wpa_wlan0\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01  5394 /dev/socket/vold\n";

const char kSampleListProcesses[] =
    "USER   PID  PPID VSIZE  RSS    WCHAN    PC         NAME\n"
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

const char* GetMockShellResponse(std::string command) {
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

  DCHECK(false) << "Should not be reached";

  return "";
}

class MockUsbEndpointDescriptor : public UsbEndpointDescriptor {
 public:
  virtual int GetAddress() const OVERRIDE { return address_; }

  virtual UsbEndpointDirection GetDirection() const OVERRIDE {
    return direction_;
  }

  virtual int GetMaximumPacketSize() const OVERRIDE {
    return maximum_packet_size_;
  }

  virtual UsbSynchronizationType GetSynchronizationType() const OVERRIDE {
    return usb_synchronization_type_;
  }

  virtual UsbTransferType GetTransferType() const OVERRIDE {
    return usb_transfer_type_;
  }
  virtual UsbUsageType GetUsageType() const OVERRIDE { return usb_usage_type_; }

  virtual int GetPollingInterval() const OVERRIDE { return polling_interval_; }

  int address_;
  UsbEndpointDirection direction_;
  int maximum_packet_size_;
  UsbSynchronizationType usb_synchronization_type_;
  UsbTransferType usb_transfer_type_;
  UsbUsageType usb_usage_type_;
  int polling_interval_;

 private:
  virtual ~MockUsbEndpointDescriptor() {}
};

template <class T>
class MockUsbInterfaceAltSettingDescriptor
    : public UsbInterfaceAltSettingDescriptor {
 public:
  MockUsbInterfaceAltSettingDescriptor(int interface_number,
                                       int alternate_setting)
      : interface_number_(interface_number),
        alternate_setting_(alternate_setting) {}

  virtual size_t GetNumEndpoints() const OVERRIDE {
    // See IsAndroidInterface function in android_usb_device.cc
    return 2;
  }

  virtual scoped_refptr<const UsbEndpointDescriptor> GetEndpoint(
      size_t index) const OVERRIDE {
    EXPECT_GT(static_cast<size_t>(2), index);
    MockUsbEndpointDescriptor* result = new MockUsbEndpointDescriptor();
    result->address_ = index + 1;
    result->usb_transfer_type_ = USB_TRANSFER_BULK;
    result->direction_ =
        ((index == 0) ? USB_DIRECTION_INBOUND : USB_DIRECTION_OUTBOUND);
    result->maximum_packet_size_ = 1 << 20;  // 1Mb maximum packet size
    return result;
  }

  virtual int GetInterfaceNumber() const OVERRIDE { return interface_number_; }

  virtual int GetAlternateSetting() const OVERRIDE {
    return alternate_setting_;
  }

  virtual int GetInterfaceClass() const OVERRIDE { return T::kClass; }

  virtual int GetInterfaceSubclass() const OVERRIDE { return T::kSubclass; }

  virtual int GetInterfaceProtocol() const OVERRIDE { return T::kProtocol; }

 protected:
  virtual ~MockUsbInterfaceAltSettingDescriptor() {};

 private:
  const int interface_number_;
  const int alternate_setting_;
};

template <class T>
class MockUsbInterfaceDescriptor : public UsbInterfaceDescriptor {
 public:
  explicit MockUsbInterfaceDescriptor(int interface_number)
      : interface_number_(interface_number) {}

  virtual size_t GetNumAltSettings() const OVERRIDE {
    // See IsAndroidInterface function in android_usb_device.cc
    return 1;
  }
  virtual scoped_refptr<const UsbInterfaceAltSettingDescriptor> GetAltSetting(
      size_t index) const OVERRIDE {
    EXPECT_EQ(static_cast<size_t>(0), index);
    return new MockUsbInterfaceAltSettingDescriptor<T>(interface_number_, 0);
  }

 protected:
  const int interface_number_;
  virtual ~MockUsbInterfaceDescriptor() {}
};

template <class T>
class MockUsbConfigDescriptor : public UsbConfigDescriptor {
 public:
  MockUsbConfigDescriptor() {}

  virtual size_t GetNumInterfaces() const OVERRIDE { return 1; }

  virtual scoped_refptr<const UsbInterfaceDescriptor> GetInterface(
      size_t index) const OVERRIDE {
    EXPECT_EQ(static_cast<size_t>(0), index);
    return new MockUsbInterfaceDescriptor<T>(index);
  }

 protected:
  virtual ~MockUsbConfigDescriptor() {};
};

template <class T>
class MockUsbDevice;

template <class T>
class MockUsbDeviceHandle : public UsbDeviceHandle {
 public:
  explicit MockUsbDeviceHandle(MockUsbDevice<T>* device)
      : device_(device),
        remaining_body_length_(0),
        next_local_socket_(0) {}

  virtual scoped_refptr<UsbDevice> GetDevice() const OVERRIDE {
    return device_;
  }

  virtual void Close() OVERRIDE { device_ = NULL; }

  bool ClaimInterface(const int interface_number) {
    if (device_->claimed_interfaces_.find(interface_number) !=
        device_->claimed_interfaces_.end())
      return false;

    device_->claimed_interfaces_.insert(interface_number);
    return true;
  }

  bool ReleaseInterface(const int interface_number) {
    if (device_->claimed_interfaces_.find(interface_number) ==
        device_->claimed_interfaces_.end())
      return false;

    device_->claimed_interfaces_.erase(interface_number);
    return true;
  }

  virtual bool SetInterfaceAlternateSetting(
      const int interface_number,
      const int alternate_setting) OVERRIDE {
    return true;
  }

  virtual bool ResetDevice() OVERRIDE { return true; }

  virtual bool GetSerial(base::string16* serial) OVERRIDE {
    *serial = base::UTF8ToUTF16(kDeviceSerial);
    return true;
  }

  // Async IO. Can be called on any thread.
  virtual void ControlTransfer(const UsbEndpointDirection direction,
                               const TransferRequestType request_type,
                               const TransferRecipient recipient,
                               const uint8 request,
                               const uint16 value,
                               const uint16 index,
                               net::IOBuffer* buffer,
                               const size_t length,
                               const unsigned int timeout,
                               const UsbTransferCallback& callback) OVERRIDE {}

  virtual void BulkTransfer(const UsbEndpointDirection direction,
                            const uint8 endpoint,
                            net::IOBuffer* buffer,
                            const size_t length,
                            const unsigned int timeout,
                            const UsbTransferCallback& callback) OVERRIDE {
    if (direction == USB_DIRECTION_OUTBOUND) {
      if (remaining_body_length_ == 0) {
        std::vector<uint32> header(6);
        memcpy(&header[0], buffer->data(), length);
        current_message_ = new AdbMessage(header[0], header[1], header[2], "");
        remaining_body_length_ = header[3];
        uint32 magic = header[5];
        if ((current_message_->command ^ 0xffffffff) != magic) {
          DCHECK(false) << "Header checksum error";
          return;
        }
      } else {
        DCHECK(current_message_);
        current_message_->body += std::string(buffer->data(), length);
        remaining_body_length_ -= length;
      }

      if (remaining_body_length_ == 0) {
        ProcessIncoming();
      }

      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback,
                     usb_service::USB_TRANSFER_COMPLETED,
                     scoped_refptr<net::IOBuffer>(),
                     0));

    } else if (direction == USB_DIRECTION_INBOUND) {
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
    DCHECK(current_message_);
    switch (current_message_->command) {
      case AdbMessage::kCommandCNXN:
        WriteResponse(new AdbMessage(AdbMessage::kCommandCNXN,
                                     kVersion,
                                     kMaxPayload,
                                     "device::ro.product.name=SampleProduct;ro."
                                     "product.model=SampleModel;ro.product."
                                     "device=SampleDevice;"));
        break;
      case AdbMessage::kCommandOPEN:
        DCHECK(current_message_->arg1 == 0);
        DCHECK(current_message_->arg0 != 0);
        if (current_message_->body.find("shell:") != std::string::npos) {
          WriteResponse(new AdbMessage(AdbMessage::kCommandOKAY,
                                       ++next_local_socket_,
                                       current_message_->arg0,
                                       ""));
          WriteResponse(
              new AdbMessage(AdbMessage::kCommandWRTE,
                             next_local_socket_,
                             current_message_->arg0,
                             GetMockShellResponse(current_message_->body.substr(
                                 0, current_message_->body.size() - 1))));
          WriteResponse(new AdbMessage(
              AdbMessage::kCommandCLSE, 0, current_message_->arg0, ""));
        }
      default:
        return;
    }
    ProcessQueries();
  }

  void WriteResponse(scoped_refptr<AdbMessage> response) {
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
                   usb_service::USB_TRANSFER_COMPLETED,
                   query.buffer,
                   query.size));
  }

  virtual void InterruptTransfer(const UsbEndpointDirection direction,
                                 const uint8 endpoint,
                                 net::IOBuffer* buffer,
                                 const size_t length,
                                 const unsigned int timeout,
                                 const UsbTransferCallback& callback) OVERRIDE {
  }

  virtual void IsochronousTransfer(
      const UsbEndpointDirection direction,
      const uint8 endpoint,
      net::IOBuffer* buffer,
      const size_t length,
      const unsigned int packets,
      const unsigned int packet_length,
      const unsigned int timeout,
      const UsbTransferCallback& callback) OVERRIDE {}

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
  scoped_refptr<AdbMessage> current_message_;
  std::vector<char> output_buffer_;
  std::queue<Query> queries_;
  int next_local_socket_;
};

template <class T>
class MockUsbDevice : public UsbDevice {
 public:
  MockUsbDevice() : UsbDevice(0, 0, 0) {}

  virtual scoped_refptr<UsbDeviceHandle> Open() OVERRIDE {
    return new MockUsbDeviceHandle<T>(this);
  }

  virtual scoped_refptr<UsbConfigDescriptor> ListInterfaces() OVERRIDE {
    return new MockUsbConfigDescriptor<T>();
  }

  virtual bool Close(scoped_refptr<UsbDeviceHandle> handle) OVERRIDE {
    return true;
  }

#if defined(OS_CHROMEOS)
  // On ChromeOS, if an interface of a claimed device is not claimed, the
  // permission broker can change the owner of the device so that the unclaimed
  // interfaces can be used. If this argument is missing, permission broker will
  // not be used and this method fails if the device is claimed.
  virtual void RequestUsbAccess(
      int interface_id,
      const base::Callback<void(bool success)>& callback) OVERRIDE {
    callback.Run(true);
  }
#endif  // OS_CHROMEOS

  std::set<int> claimed_interfaces_;

 protected:
  virtual ~MockUsbDevice() {}
};

class MockUsbService : public UsbService {
 public:
  MockUsbService() {
    devices_.push_back(new MockUsbDevice<AndroidTraits>());
  }

  virtual ~MockUsbService() {}

  virtual scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  virtual void GetDevices(
      std::vector<scoped_refptr<UsbDevice> >* devices) OVERRIDE {
    STLClearObject(devices);
    std::copy(devices_.begin(), devices_.end(), back_inserter(*devices));
  }

  std::vector<scoped_refptr<UsbDevice> > devices_;
};

class MockUsbServiceForCheckingTraits : public UsbService {
 public:
  MockUsbServiceForCheckingTraits() : step_(0) {}

  virtual ~MockUsbServiceForCheckingTraits() {}

  virtual scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

  virtual void GetDevices(
      std::vector<scoped_refptr<UsbDevice> >* devices) OVERRIDE {
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
                              scoped_refptr<DevToolsAndroidBridge> adb_bridge)
      : closure_(closure), adb_bridge_(adb_bridge) {}

  virtual void DeviceCountChanged(int count) OVERRIDE {
    adb_bridge_->RemoveDeviceCountListener(this);
    closure_.Run();
  }

  base::Closure closure_;
  scoped_refptr<DevToolsAndroidBridge> adb_bridge_;
};

class AndroidUsbDiscoveryTest : public InProcessBrowserTest {
 protected:
  AndroidUsbDiscoveryTest()
      : scheduler_invoked_(0) {
  }
  virtual void SetUpOnMainThread() OVERRIDE {
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

  virtual void TearDownOnMainThread() OVERRIDE {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    UsbService* service = NULL;
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&UsbService::SetInstanceForTest, service),
        runner->QuitClosure());
    runner->Run();
  }

  scoped_refptr<content::MessageLoopRunner> runner_;
  scoped_refptr<DevToolsAndroidBridge> adb_bridge_;
  int scheduler_invoked_;
};

class AndroidUsbCountTest : public AndroidUsbDiscoveryTest {
 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    AndroidUsbDiscoveryTest::SetUpOnMainThread();
    DevToolsAndroidBridgeWarmUp warmup(runner_->QuitClosure(), adb_bridge_);
    adb_bridge_->AddDeviceCountListener(&warmup);
    runner_->Run();
    runner_ = new content::MessageLoopRunner;
  }
};

class AndroidUsbTraitsTest : public AndroidUsbDiscoveryTest {
 protected:
  virtual void SetUpService() OVERRIDE {
    UsbService::SetInstanceForTest(new MockUsbServiceForCheckingTraits());
  }
};

class MockListListener : public DevToolsAndroidBridge::DeviceListListener {
 public:
  MockListListener(scoped_refptr<DevToolsAndroidBridge> adb_bridge,
                   const base::Closure& callback)
      : adb_bridge_(adb_bridge),
        callback_(callback) {
  }

  virtual void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) OVERRIDE {
    if (devices.size() > 0) {
      if (devices[0]->is_connected()) {
        ASSERT_EQ(kDeviceModel, devices[0]->model());
        ASSERT_EQ(kDeviceSerial, devices[0]->serial());
        adb_bridge_->RemoveDeviceListListener(this);
        callback_.Run();
      }
    }
  }

  scoped_refptr<DevToolsAndroidBridge> adb_bridge_;
  base::Closure callback_;
};

class MockCountListener : public DevToolsAndroidBridge::DeviceCountListener {
 public:
  explicit MockCountListener(scoped_refptr<DevToolsAndroidBridge> adb_bridge)
      : adb_bridge_(adb_bridge),
        reposts_left_(10),
        invoked_(0) {
  }

  virtual void DeviceCountChanged(int count) OVERRIDE {
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

  scoped_refptr<DevToolsAndroidBridge> adb_bridge_;
  int reposts_left_;
  int invoked_;
};

class MockCountListenerWithReAdd : public MockCountListener {
 public:
  explicit MockCountListenerWithReAdd(
      scoped_refptr<DevToolsAndroidBridge> adb_bridge)
      : MockCountListener(adb_bridge),
        readd_count_(2) {
  }

  virtual void DeviceCountChanged(int count) OVERRIDE {
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
      scoped_refptr<DevToolsAndroidBridge> adb_bridge)
      : MockCountListener(adb_bridge),
        readded_(false) {
  }

  virtual void DeviceCountChanged(int count) OVERRIDE {
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
      scoped_refptr<DevToolsAndroidBridge> adb_bridge)
      : MockCountListener(adb_bridge),
        step_(0) {
  }
  virtual void DeviceCountChanged(int count) OVERRIDE {
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
