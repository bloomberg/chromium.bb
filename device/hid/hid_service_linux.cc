// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_service_linux.h"

#include <fcntl.h>
#include <limits>
#include <string>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/scoped_observer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "components/device_event_log/device_event_log.h"
#include "device/hid/device_monitor_linux.h"
#include "device/hid/hid_connection_linux.h"
#include "device/hid/hid_device_info_linux.h"
#include "device/udev_linux/scoped_udev.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#endif  // defined(OS_CHROMEOS)

namespace device {

namespace {

const char kHidrawSubsystem[] = "hidraw";
const char kHIDID[] = "HID_ID";
const char kHIDName[] = "HID_NAME";
const char kHIDUnique[] = "HID_UNIQ";
const char kSysfsReportDescriptorKey[] = "report_descriptor";

}  // namespace

struct HidServiceLinux::ConnectParams {
  ConnectParams(scoped_refptr<HidDeviceInfoLinux> device_info,
                const ConnectCallback& callback,
                scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
      : device_info(device_info),
        callback(callback),
        task_runner(task_runner),
        file_task_runner(file_task_runner) {}
  ~ConnectParams() {}

  scoped_refptr<HidDeviceInfoLinux> device_info;
  ConnectCallback callback;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner;
  base::File device_file;
};

class HidServiceLinux::FileThreadHelper
    : public DeviceMonitorLinux::Observer,
      public base::MessageLoop::DestructionObserver {
 public:
  FileThreadHelper(base::WeakPtr<HidServiceLinux> service,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : observer_(this), service_(service), task_runner_(task_runner) {
    DeviceMonitorLinux* monitor = DeviceMonitorLinux::GetInstance();
    observer_.Add(monitor);
    monitor->Enumerate(
        base::Bind(&FileThreadHelper::OnDeviceAdded, base::Unretained(this)));
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(&HidServiceLinux::FirstEnumerationComplete, service_));
  }

  ~FileThreadHelper() override {
    DCHECK(thread_checker_.CalledOnValidThread());
  }

 private:
  // DeviceMonitorLinux::Observer:
  void OnDeviceAdded(udev_device* device) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    const char* device_path = udev_device_get_syspath(device);
    if (!device_path) {
      return;
    }
    HidDeviceId device_id = device_path;

    const char* subsystem = udev_device_get_subsystem(device);
    if (!subsystem || strcmp(subsystem, kHidrawSubsystem) != 0) {
      return;
    }

    const char* str_property = udev_device_get_devnode(device);
    if (!str_property) {
      return;
    }
    std::string device_node = str_property;

    udev_device* parent = udev_device_get_parent(device);
    if (!parent) {
      return;
    }

    const char* hid_id = udev_device_get_property_value(parent, kHIDID);
    if (!hid_id) {
      return;
    }

    std::vector<std::string> parts;
    base::SplitString(hid_id, ':', &parts);
    if (parts.size() != 3) {
      return;
    }

    uint32_t int_property = 0;
    if (!HexStringToUInt(base::StringPiece(parts[1]), &int_property) ||
        int_property > std::numeric_limits<uint16_t>::max()) {
      return;
    }
    uint16_t vendor_id = int_property;

    if (!HexStringToUInt(base::StringPiece(parts[2]), &int_property) ||
        int_property > std::numeric_limits<uint16_t>::max()) {
      return;
    }
    uint16_t product_id = int_property;

    std::string serial_number;
    str_property = udev_device_get_property_value(parent, kHIDUnique);
    if (str_property != NULL) {
      serial_number = str_property;
    }

    std::string product_name;
    str_property = udev_device_get_property_value(parent, kHIDName);
    if (str_property != NULL) {
      product_name = str_property;
    }

    const char* parent_sysfs_path = udev_device_get_syspath(parent);
    if (!parent_sysfs_path) {
      return;
    }
    base::FilePath report_descriptor_path =
        base::FilePath(parent_sysfs_path).Append(kSysfsReportDescriptorKey);
    std::string report_descriptor_str;
    if (!base::ReadFileToString(report_descriptor_path,
                                &report_descriptor_str)) {
      return;
    }

    scoped_refptr<HidDeviceInfo> device_info(new HidDeviceInfoLinux(
        device_id, device_node, vendor_id, product_id, product_name,
        serial_number,
        kHIDBusTypeUSB,  // TODO(reillyg): Detect Bluetooth. crbug.com/443335
        std::vector<uint8>(report_descriptor_str.begin(),
                           report_descriptor_str.end())));

    task_runner_->PostTask(FROM_HERE, base::Bind(&HidServiceLinux::AddDevice,
                                                 service_, device_info));
  }

  void OnDeviceRemoved(udev_device* device) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    const char* device_path = udev_device_get_syspath(device);
    if (device_path) {
      task_runner_->PostTask(
          FROM_HERE, base::Bind(&HidServiceLinux::RemoveDevice, service_,
                                std::string(device_path)));
    }
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    base::MessageLoop::current()->RemoveDestructionObserver(this);
    delete this;
  }

  base::ThreadChecker thread_checker_;
  ScopedObserver<DeviceMonitorLinux, DeviceMonitorLinux::Observer> observer_;

  // This weak pointer is only valid when checked on this task runner.
  base::WeakPtr<HidServiceLinux> service_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

HidServiceLinux::HidServiceLinux(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(file_task_runner), weak_factory_(this) {
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  // The device watcher is passed a weak pointer back to this service so that it
  // can be cleaned up after the service is destroyed however this weak pointer
  // must be constructed on the this thread where it will be checked.
  file_task_runner_->PostTask(
      FROM_HERE, base::Bind(&HidServiceLinux::StartHelper,
                            weak_factory_.GetWeakPtr(), task_runner_));
}

// static
void HidServiceLinux::StartHelper(
    base::WeakPtr<HidServiceLinux> weak_ptr,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  // Helper is a message loop destruction observer and will delete itself when
  // this thread's message loop is destroyed.
  new FileThreadHelper(weak_ptr, task_runner);
}

void HidServiceLinux::Connect(const HidDeviceId& device_id,
                              const ConnectCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto& map_entry = devices().find(device_id);
  if (map_entry == devices().end()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    return;
  }
  scoped_refptr<HidDeviceInfoLinux> device_info =
      static_cast<HidDeviceInfoLinux*>(map_entry->second.get());

  scoped_ptr<ConnectParams> params(new ConnectParams(
      device_info, callback, task_runner_, file_task_runner_));

#if defined(OS_CHROMEOS)
  if (base::SysInfo::IsRunningOnChromeOS()) {
    chromeos::PermissionBrokerClient* client =
        chromeos::DBusThreadManager::Get()->GetPermissionBrokerClient();
    DCHECK(client) << "Could not get permission broker client.";
    if (client) {
      client->RequestPathAccess(
          device_info->device_node(), -1,
          base::Bind(&HidServiceLinux::OnRequestPathAccessComplete,
                     base::Passed(&params)));
    } else {
      task_runner_->PostTask(FROM_HERE, base::Bind(callback, nullptr));
    }
    return;
  }
#endif  // defined(OS_CHROMEOS)

  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&HidServiceLinux::OpenDevice, base::Passed(&params)));
}

HidServiceLinux::~HidServiceLinux() {
  file_task_runner_->DeleteSoon(FROM_HERE, helper_.release());
}

#if defined(OS_CHROMEOS)
// static
void HidServiceLinux::OnRequestPathAccessComplete(
    scoped_ptr<ConnectParams> params,
    bool success) {
  if (success) {
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner =
        params->file_task_runner;
    file_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&HidServiceLinux::OpenDevice, base::Passed(&params)));
  } else {
    params->callback.Run(nullptr);
  }
}
#endif  // defined(OS_CHROMEOS)

// static
void HidServiceLinux::OpenDevice(scoped_ptr<ConnectParams> params) {
  base::ThreadRestrictions::AssertIOAllowed();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner = params->task_runner;
  base::FilePath device_path(params->device_info->device_node());
  base::File& device_file = params->device_file;
  int flags =
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE;
  device_file.Initialize(device_path, flags);
  if (!device_file.IsValid()) {
    base::File::Error file_error = device_file.error_details();

    if (file_error == base::File::FILE_ERROR_ACCESS_DENIED) {
      HID_LOG(EVENT)
          << "Access denied opening device read-write, trying read-only.";
      flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
      device_file.Initialize(device_path, flags);
    }
  }
  if (!device_file.IsValid()) {
    HID_LOG(EVENT) << "Failed to open '" << params->device_info->device_node()
                   << "': "
                   << base::File::ErrorToString(device_file.error_details());
    task_runner->PostTask(FROM_HERE, base::Bind(params->callback, nullptr));
    return;
  }

  int result = fcntl(device_file.GetPlatformFile(), F_GETFL);
  if (result == -1) {
    HID_PLOG(ERROR) << "Failed to get flags from the device file descriptor";
    task_runner->PostTask(FROM_HERE, base::Bind(params->callback, nullptr));
    return;
  }

  result = fcntl(device_file.GetPlatformFile(), F_SETFL, result | O_NONBLOCK);
  if (result == -1) {
    HID_PLOG(ERROR) << "Failed to set the non-blocking flag on the device fd";
    task_runner->PostTask(FROM_HERE, base::Bind(params->callback, nullptr));
    return;
  }

  task_runner->PostTask(FROM_HERE, base::Bind(&HidServiceLinux::ConnectImpl,
                                              base::Passed(&params)));
}

// static
void HidServiceLinux::ConnectImpl(scoped_ptr<ConnectParams> params) {
  DCHECK(params->device_file.IsValid());
  params->callback.Run(make_scoped_refptr(
      new HidConnectionLinux(params->device_info, params->device_file.Pass(),
                             params->file_task_runner)));
}

}  // namespace device
