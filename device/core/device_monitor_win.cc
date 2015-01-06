// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/core/device_monitor_win.h"

#include <dbt.h>
#include <map>
#include <windows.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/message_window.h"

namespace device {

class DeviceMonitorMessageWindow;

namespace {

const wchar_t kWindowClassName[] = L"DeviceMonitorMessageWindow";
DeviceMonitorMessageWindow* g_message_window;

// Provides basic comparability for GUIDs so that they can be used as keys to an
// STL map.
struct CompareGUID {
  bool operator()(const GUID& a, const GUID& b) {
    return memcmp(&a, &b, sizeof a) < 0;
  }
};
}

// This singleton class manages a shared message window for all registered
// device notification observers. It vends one instance of DeviceManagerWin for
// each unique GUID it sees.
class DeviceMonitorMessageWindow {
 public:
  static DeviceMonitorMessageWindow* GetInstance() {
    if (!g_message_window) {
      g_message_window = new DeviceMonitorMessageWindow();
      base::AtExitManager::RegisterTask(
          base::Bind(&base::DeletePointer<DeviceMonitorMessageWindow>,
                     base::Unretained(g_message_window)));
    }
    return g_message_window;
  }

  DeviceMonitorWin* GetForDeviceInterface(const GUID& device_interface) {
    scoped_ptr<DeviceMonitorWin>& device_monitor =
        device_monitors_[device_interface];
    if (!device_monitor) {
      DEV_BROADCAST_DEVICEINTERFACE db = {sizeof(DEV_BROADCAST_DEVICEINTERFACE),
                                          DBT_DEVTYP_DEVICEINTERFACE,
                                          0,
                                          device_interface};
      HDEVNOTIFY notify_handle = RegisterDeviceNotification(
          window_->hwnd(), &db, DEVICE_NOTIFY_WINDOW_HANDLE);
      if (!notify_handle) {
        LOG(ERROR) << "Failed to register for device notifications.";
        return nullptr;
      }
      device_monitor.reset(new DeviceMonitorWin(notify_handle));
    }
    return device_monitor.get();
  }

 private:
  friend void base::DeletePointer<DeviceMonitorMessageWindow>(
      DeviceMonitorMessageWindow* message_window);

  DeviceMonitorMessageWindow() {
    window_.reset(new base::win::MessageWindow());
    if (!window_->CreateNamed(
            base::Bind(&DeviceMonitorMessageWindow::HandleMessage,
                       base::Unretained(this)),
            base::string16(kWindowClassName))) {
      LOG(ERROR) << "Failed to create message window: " << kWindowClassName;
      window_.reset();
    }
  }

  ~DeviceMonitorMessageWindow() {}

  bool HandleMessage(UINT message,
                     WPARAM wparam,
                     LPARAM lparam,
                     LRESULT* result) {
    if (message == WM_DEVICECHANGE) {
      DEV_BROADCAST_DEVICEINTERFACE* db =
          reinterpret_cast<DEV_BROADCAST_DEVICEINTERFACE*>(lparam);
      const auto& map_entry = device_monitors_.find(db->dbcc_classguid);
      if (map_entry == device_monitors_.end()) {
        return false;
      }
      DeviceMonitorWin* device_monitor = map_entry->second.get();

      std::string device_path(base::SysWideToUTF8(db->dbcc_name));
      DCHECK(base::IsStringASCII(device_path));
      if (wparam == DBT_DEVICEARRIVAL) {
        device_monitor->NotifyDeviceAdded(
            base::StringToLowerASCII(device_path));
      } else if (wparam == DBT_DEVICEREMOVECOMPLETE) {
        device_monitor->NotifyDeviceRemoved(
            base::StringToLowerASCII(device_path));
      } else {
        return false;
      }
      *result = NULL;
      return true;
    }
    return false;
  }

  std::map<GUID, scoped_ptr<DeviceMonitorWin>, CompareGUID> device_monitors_;
  scoped_ptr<base::win::MessageWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMonitorMessageWindow);
};

void DeviceMonitorWin::Observer::OnDeviceAdded(const std::string& device_path) {
}

void DeviceMonitorWin::Observer::OnDeviceRemoved(
    const std::string& device_path) {
}

// static
DeviceMonitorWin* DeviceMonitorWin::GetForDeviceInterface(
    const GUID& device_interface) {
  DeviceMonitorMessageWindow* message_window =
      DeviceMonitorMessageWindow::GetInstance();
  return message_window->GetForDeviceInterface(device_interface);
}

DeviceMonitorWin::~DeviceMonitorWin() {
  UnregisterDeviceNotification(notify_handle_);
}

void DeviceMonitorWin::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DeviceMonitorWin::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

DeviceMonitorWin::DeviceMonitorWin(HDEVNOTIFY notify_handle)
    : notify_handle_(notify_handle) {
}

void DeviceMonitorWin::NotifyDeviceAdded(const std::string& device_path) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceAdded(device_path));
}

void DeviceMonitorWin::NotifyDeviceRemoved(const std::string& device_path) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnDeviceRemoved(device_path));
}
}