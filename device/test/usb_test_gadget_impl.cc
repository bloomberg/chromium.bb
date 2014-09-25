// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/test/usb_test_gadget.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "device/usb/usb_device.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_service.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

using ::base::PlatformThread;
using ::base::TimeDelta;

namespace device {

namespace {

static const char kCommandLineSwitch[] = "enable-gadget-tests";
static const int kClaimRetries = 100;  // 5 seconds
static const int kDisconnectRetries = 100;  // 5 seconds
static const int kRetryPeriod = 50;  // 0.05 seconds
static const int kReconnectRetries = 100;  // 5 seconds
static const int kUpdateRetries = 100;  // 5 seconds

struct UsbTestGadgetConfiguration {
  UsbTestGadget::Type type;
  const char* http_resource;
  uint16 product_id;
};

static const struct UsbTestGadgetConfiguration kConfigurations[] = {
    {UsbTestGadget::DEFAULT, "/unconfigure", 0x58F0},
    {UsbTestGadget::KEYBOARD, "/keyboard/configure", 0x58F1},
    {UsbTestGadget::MOUSE, "/mouse/configure", 0x58F2},
    {UsbTestGadget::HID_ECHO, "/hid_echo/configure", 0x58F3},
    {UsbTestGadget::ECHO, "/echo/configure", 0x58F4},
};

class UsbTestGadgetImpl : public UsbTestGadget {
 public:
  virtual ~UsbTestGadgetImpl();

  virtual bool Unclaim() OVERRIDE;
  virtual bool Disconnect() OVERRIDE;
  virtual bool Reconnect() OVERRIDE;
  virtual bool SetType(Type type) OVERRIDE;
  virtual UsbDevice* GetDevice() const OVERRIDE;
  virtual std::string GetSerialNumber() const OVERRIDE;

 protected:
  UsbTestGadgetImpl();

 private:
  scoped_ptr<net::URLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* delegate);
  int SimplePOSTRequest(const GURL& url, const std::string& form_data);
  bool FindUnclaimed();
  bool GetVersion(std::string* version);
  bool Update();
  bool FindClaimed();
  bool ReadLocalVersion(std::string* version);
  bool ReadLocalPackage(std::string* package);
  bool ReadFile(const base::FilePath& file_path, std::string* content);

  class Delegate : public net::URLFetcherDelegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    void WaitForCompletion() {
      run_loop_.Run();
    }

    virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
      run_loop_.Quit();
    }

   private:
    base::RunLoop run_loop_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  scoped_refptr<UsbDevice> device_;
  std::string device_address_;
  scoped_ptr<net::URLRequestContext> request_context_;
  std::string session_id_;
  UsbService* usb_service_;

  friend class UsbTestGadget;

  DISALLOW_COPY_AND_ASSIGN(UsbTestGadgetImpl);
};

}  // namespace

bool UsbTestGadget::IsTestEnabled() {
  base::CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kCommandLineSwitch);
}

scoped_ptr<UsbTestGadget> UsbTestGadget::Claim() {
  scoped_ptr<UsbTestGadgetImpl> gadget(new UsbTestGadgetImpl);

  int retries = kClaimRetries;
  while (!gadget->FindUnclaimed()) {
    if (--retries == 0) {
      LOG(ERROR) << "Failed to find an unclaimed device.";
      return scoped_ptr<UsbTestGadget>();
    }
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(kRetryPeriod));
  }
  VLOG(1) << "It took " << (kClaimRetries - retries)
          << " retries to find an unclaimed device.";

  return gadget.PassAs<UsbTestGadget>();
}

UsbTestGadgetImpl::UsbTestGadgetImpl() {
  net::URLRequestContextBuilder context_builder;
  context_builder.set_proxy_service(net::ProxyService::CreateDirect());
  request_context_.reset(context_builder.Build());

  base::ProcessId process_id = base::Process::Current().pid();
  session_id_ = base::StringPrintf(
      "%s:%p", base::HexEncode(&process_id, sizeof(process_id)).c_str(), this);

  usb_service_ = UsbService::GetInstance(NULL);
}

UsbTestGadgetImpl::~UsbTestGadgetImpl() {
  if (!device_address_.empty()) {
    Unclaim();
  }
}

UsbDevice* UsbTestGadgetImpl::GetDevice() const {
  return device_.get();
}

std::string UsbTestGadgetImpl::GetSerialNumber() const {
  return device_address_;
}

scoped_ptr<net::URLFetcher> UsbTestGadgetImpl::CreateURLFetcher(
    const GURL& url, net::URLFetcher::RequestType request_type,
    net::URLFetcherDelegate* delegate) {
  scoped_ptr<net::URLFetcher> url_fetcher(
      net::URLFetcher::Create(url, request_type, delegate));

  url_fetcher->SetRequestContext(
      new net::TrivialURLRequestContextGetter(
          request_context_.get(),
          base::MessageLoop::current()->message_loop_proxy()));

  return url_fetcher.PassAs<net::URLFetcher>();
}

int UsbTestGadgetImpl::SimplePOSTRequest(const GURL& url,
                                         const std::string& form_data) {
  Delegate delegate;
  scoped_ptr<net::URLFetcher> url_fetcher =
    CreateURLFetcher(url, net::URLFetcher::POST, &delegate);

  url_fetcher->SetUploadData("application/x-www-form-urlencoded", form_data);
  url_fetcher->Start();
  delegate.WaitForCompletion();

  return url_fetcher->GetResponseCode();
}

bool UsbTestGadgetImpl::FindUnclaimed() {
  std::vector<scoped_refptr<UsbDevice> > devices;
  usb_service_->GetDevices(&devices);

  for (std::vector<scoped_refptr<UsbDevice> >::const_iterator iter =
         devices.begin(); iter != devices.end(); ++iter) {
    const scoped_refptr<UsbDevice> &device = *iter;
    if (device->vendor_id() == 0x18D1 && device->product_id() == 0x58F0) {
      base::string16 serial_utf16;
      if (!device->GetSerialNumber(&serial_utf16)) {
        continue;
      }

      const std::string serial = base::UTF16ToUTF8(serial_utf16);
      const GURL url("http://" + serial + "/claim");
      const std::string form_data = base::StringPrintf(
          "session_id=%s",
          net::EscapeUrlEncodedData(session_id_, true).c_str());
      const int response_code = SimplePOSTRequest(url, form_data);

      if (response_code == 200) {
        device_address_ = serial;
        device_ = device;
        break;
      }

      // The device is probably claimed by another process.
      if (response_code != 403) {
        LOG(WARNING) << "Unexpected HTTP " << response_code << " from /claim.";
      }
    }
  }

  std::string local_version;
  std::string version;
  if (!ReadLocalVersion(&local_version) ||
      !GetVersion(&version)) {
    return false;
  }

  if (version == local_version) {
    return true;
  }

  return Update();
}

bool UsbTestGadgetImpl::GetVersion(std::string* version) {
  Delegate delegate;
  const GURL url("http://" + device_address_ + "/version");
  scoped_ptr<net::URLFetcher> url_fetcher =
      CreateURLFetcher(url, net::URLFetcher::GET, &delegate);

  url_fetcher->Start();
  delegate.WaitForCompletion();

  const int response_code = url_fetcher->GetResponseCode();
  if (response_code != 200) {
    VLOG(2) << "Unexpected HTTP " << response_code << " from /version.";
    return false;
  }

  STLClearObject(version);
  if (!url_fetcher->GetResponseAsString(version)) {
    VLOG(2) << "Failed to read body from /version.";
    return false;
  }
  return true;
}

bool UsbTestGadgetImpl::Update() {
  std::string version;
  if (!ReadLocalVersion(&version)) {
    return false;
  }
  LOG(INFO) << "Updating " << device_address_ << " to " << version << "...";

  Delegate delegate;
  const GURL url("http://" + device_address_ + "/update");
  scoped_ptr<net::URLFetcher> url_fetcher =
      CreateURLFetcher(url, net::URLFetcher::POST, &delegate);

  const std::string mime_header =
      base::StringPrintf(
      "--foo\r\n"
      "Content-Disposition: form-data; name=\"file\"; "
          "filename=\"usb_gadget-%s.zip\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n", version.c_str());
  const std::string mime_footer("\r\n--foo--\r\n");

  std::string package;
  if (!ReadLocalPackage(&package)) {
    return false;
  }

  url_fetcher->SetUploadData("multipart/form-data; boundary=foo",
                             mime_header + package + mime_footer);
  url_fetcher->Start();
  delegate.WaitForCompletion();

  const int response_code = url_fetcher->GetResponseCode();
  if (response_code != 200) {
    LOG(ERROR) << "Unexpected HTTP " << response_code << " from /update.";
    return false;
  }

  int retries = kUpdateRetries;
  std::string new_version;
  while (!GetVersion(&new_version) || new_version != version) {
    if (--retries == 0) {
      LOG(ERROR) << "Device not responding with new version.";
      return false;
    }
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(kRetryPeriod));
  }
  VLOG(1) << "It took " << (kUpdateRetries - retries)
          << " retries to see the new version.";

  // Release the old reference to the device and try to open a new one.
  device_ = NULL;
  retries = kReconnectRetries;
  while (!FindClaimed()) {
    if (--retries == 0) {
      LOG(ERROR) << "Failed to find updated device.";
      return false;
    }
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(kRetryPeriod));
  }
  VLOG(1) << "It took " << (kReconnectRetries - retries)
          << " retries to find the updated device.";

  return true;
}

bool UsbTestGadgetImpl::FindClaimed() {
  CHECK(!device_.get());

  std::string expected_serial = GetSerialNumber();

  std::vector<scoped_refptr<UsbDevice> > devices;
  usb_service_->GetDevices(&devices);

  for (std::vector<scoped_refptr<UsbDevice> >::iterator iter =
         devices.begin(); iter != devices.end(); ++iter) {
    scoped_refptr<UsbDevice> &device = *iter;

    if (device->vendor_id() == 0x18D1) {
      const uint16 product_id = device->product_id();
      bool found = false;
      for (size_t i = 0; i < arraysize(kConfigurations); ++i) {
        if (product_id == kConfigurations[i].product_id) {
          found = true;
          break;
        }
      }
      if (!found) {
        continue;
      }

      base::string16 serial_utf16;
      if (!device->GetSerialNumber(&serial_utf16)) {
        continue;
      }

      std::string serial = base::UTF16ToUTF8(serial_utf16);
      if (serial != expected_serial) {
        continue;
      }

      device_ = device;
      return true;
    }
  }

  return false;
}

bool UsbTestGadgetImpl::ReadLocalVersion(std::string* version) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_EXE, &file_path));
  file_path = file_path.AppendASCII("usb_gadget.zip.md5");

  return ReadFile(file_path, version);
}

bool UsbTestGadgetImpl::ReadLocalPackage(std::string* package) {
  base::FilePath file_path;
  CHECK(PathService::Get(base::DIR_EXE, &file_path));
  file_path = file_path.AppendASCII("usb_gadget.zip");

  return ReadFile(file_path, package);
}

bool UsbTestGadgetImpl::ReadFile(const base::FilePath& file_path,
                                 std::string* content) {
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "Cannot open " << file_path.MaybeAsASCII() << ": "
               << base::File::ErrorToString(file.error_details());
    return false;
  }

  STLClearObject(content);
  int rv;
  do {
    char buf[4096];
    rv = file.ReadAtCurrentPos(buf, sizeof buf);
    if (rv == -1) {
      LOG(ERROR) << "Cannot read " << file_path.MaybeAsASCII() << ": "
                 << base::File::ErrorToString(file.error_details());
      return false;
    }
    content->append(buf, rv);
  } while (rv > 0);

  return true;
}

bool UsbTestGadgetImpl::Unclaim() {
  VLOG(1) << "Releasing the device at " << device_address_ << ".";

  const GURL url("http://" + device_address_ + "/unclaim");
  const int response_code = SimplePOSTRequest(url, "");

  if (response_code != 200) {
    LOG(ERROR) << "Unexpected HTTP " << response_code << " from /unclaim.";
    return false;
  }
  return true;
}

bool UsbTestGadgetImpl::SetType(Type type) {
  const struct UsbTestGadgetConfiguration* config = NULL;
  for (size_t i = 0; i < arraysize(kConfigurations); ++i) {
    if (kConfigurations[i].type == type) {
      config = &kConfigurations[i];
    }
  }
  CHECK(config);

  const GURL url("http://" + device_address_ + config->http_resource);
  const int response_code = SimplePOSTRequest(url, "");

  if (response_code != 200) {
    LOG(ERROR) << "Unexpected HTTP " << response_code
               << " from " << config->http_resource << ".";
    return false;
  }

  // Release the old reference to the device and try to open a new one.
  int retries = kReconnectRetries;
  while (true) {
    device_ = NULL;
    if (FindClaimed() && device_->product_id() == config->product_id) {
      break;
    }
    if (--retries == 0) {
      LOG(ERROR) << "Failed to find updated device.";
      return false;
    }
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(kRetryPeriod));
  }
  VLOG(1) << "It took " << (kReconnectRetries - retries)
          << " retries to find the updated device.";

  return true;
}

bool UsbTestGadgetImpl::Disconnect() {
  const GURL url("http://" + device_address_ + "/disconnect");
  const int response_code = SimplePOSTRequest(url, "");

  if (response_code != 200) {
    LOG(ERROR) << "Unexpected HTTP " << response_code << " from /disconnect.";
    return false;
  }

  // Release the old reference to the device and wait until it can't be found.
  int retries = kDisconnectRetries;
  while (true) {
    device_ = NULL;
    if (!FindClaimed()) {
      break;
    }
    if (--retries == 0) {
      LOG(ERROR) << "Device did not disconnect.";
      return false;
    }
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(kRetryPeriod));
  }
  VLOG(1) << "It took " << (kDisconnectRetries - retries)
          << " retries for the device to disconnect.";

  return true;
}

bool UsbTestGadgetImpl::Reconnect() {
  const GURL url("http://" + device_address_ + "/reconnect");
  const int response_code = SimplePOSTRequest(url, "");

  if (response_code != 200) {
    LOG(ERROR) << "Unexpected HTTP " << response_code << " from /reconnect.";
    return false;
  }

  int retries = kDisconnectRetries;
  while (true) {
    if (FindClaimed()) {
      break;
    }
    if (--retries == 0) {
      LOG(ERROR) << "Device did not reconnect.";
      return false;
    }
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(kRetryPeriod));
  }
  VLOG(1) << "It took " << (kDisconnectRetries - retries)
          << " retries for the device to reconnect.";

  return true;
}

}  // namespace device
