// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/dynamically_initialized_midi_manager_win.h"

#include <windows.h>

#include <mmreg.h>
#include <mmsystem.h>

#include <algorithm>
#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "media/midi/midi_port_info.h"
#include "media/midi/midi_service.h"

namespace midi {

namespace {

// Global variables to identify MidiManager instance.
constexpr int kInvalidInstanceId = -1;
int g_active_instance_id = kInvalidInstanceId;
DynamicallyInitializedMidiManagerWin* g_manager_instance = nullptr;

// Obtains base::Lock instance pointer to lock instance_id.
base::Lock* GetInstanceIdLock() {
  static base::Lock* lock = new base::Lock;
  return lock;
}

// Issues unique MidiManager instance ID.
int IssueNextInstanceId() {
  static int id = kInvalidInstanceId;
  return ++id;
}

// Use single TaskRunner for all tasks running outside the I/O thread.
constexpr int kTaskRunner = 0;

// Obtains base::Lock instance pointer to ensure tasks run safely on TaskRunner.
base::Lock* GetTaskLock() {
  static base::Lock* lock = new base::Lock;
  return lock;
}

// Helper function to run a posted task on TaskRunner safely.
void RunTask(int instance_id, const base::Closure& task) {
  // Obtains task lock to ensure that the instance should not complete
  // Finalize() while running the |task|.
  base::AutoLock task_lock(*GetTaskLock());
  {
    // If Finalize() finished before the lock avobe, do nothing.
    base::AutoLock lock(*GetInstanceIdLock());
    if (instance_id != g_active_instance_id)
      return;
  }
  task.Run();
}

// TODO(toyoshim): Factor out TaskRunner related functionaliries above, and
// deprecate MidiScheduler. It should be available via MidiManager::scheduler().

class Port {
 public:
  Port(const std::string& type,
       uint32_t device_id,
       uint16_t manufacturer_id,
       uint16_t product_id,
       uint32_t driver_version,
       const std::string& product_name)
      : index_(0u),
        type_(type),
        device_id_(device_id),
        manufacturer_id_(manufacturer_id),
        product_id_(product_id),
        driver_version_(driver_version),
        product_name_(product_name) {
    info_.manufacturer = "unknown";  // TODO(toyoshim): Use USB information.
    info_.name = product_name_;
    info_.version = base::StringPrintf("%d.%d", HIBYTE(driver_version_),
                                       LOBYTE(driver_version_));
    info_.state = mojom::PortState::DISCONNECTED;
  }

  virtual ~Port() {}

  bool operator==(const Port& other) const {
    // Should not use |device_id| for comparison because it can be changed on
    // each enumeration.
    // Since the GUID will be changed on each enumeration for Microsoft GS
    // Wavetable synth and might be done for others, do not use it for device
    // comparison.
    return manufacturer_id_ == other.manufacturer_id_ &&
           product_id_ == other.product_id_ &&
           driver_version_ == other.driver_version_ &&
           product_name_ == other.product_name_;
  }

  bool IsConnected() const {
    return info_.state != mojom::PortState::DISCONNECTED;
  }

  void set_index(size_t index) {
    index_ = index;
    // TODO(toyoshim): Use hashed ID.
    info_.id = base::StringPrintf("%s-%d", type_.c_str(), index_);
  }
  size_t index() { return index_; }
  void set_device_id(uint32_t device_id) { device_id_ = device_id; }
  uint32_t device_id() { return device_id_; }
  const MidiPortInfo& info() { return info_; }

  virtual bool Connect() {
    if (info_.state != mojom::PortState::DISCONNECTED)
      return false;

    info_.state = mojom::PortState::CONNECTED;
    // TODO(toyoshim) Until open() / close() are supported, open each device on
    // connected.
    Open();
    return true;
  }

  virtual bool Disconnect() {
    if (info_.state == mojom::PortState::DISCONNECTED)
      return false;
    info_.state = mojom::PortState::DISCONNECTED;
    return true;
  }

  virtual void Open() { info_.state = mojom::PortState::OPENED; }

 protected:
  size_t index_;
  std::string type_;
  uint32_t device_id_;
  const uint16_t manufacturer_id_;
  const uint16_t product_id_;
  const uint32_t driver_version_;
  const std::string product_name_;
  MidiPortInfo info_;
};  // class Port

}  // namespace

// TODO(toyoshim): Following patches will implement actual functions.
class DynamicallyInitializedMidiManagerWin::InPort final : public Port {
 public:
  InPort(UINT device_id, const MIDIINCAPS2W& caps)
      : Port("input",
             device_id,
             caps.wMid,
             caps.wPid,
             caps.vDriverVersion,
             base::WideToUTF8(
                 base::string16(caps.szPname, wcslen(caps.szPname)))) {}

  static std::vector<std::unique_ptr<InPort>> EnumerateActivePorts() {
    std::vector<std::unique_ptr<InPort>> ports;
    const UINT num_devices = midiInGetNumDevs();
    for (UINT device_id = 0; device_id < num_devices; ++device_id) {
      MIDIINCAPS2W caps;
      MMRESULT result = midiInGetDevCaps(
          device_id, reinterpret_cast<LPMIDIINCAPSW>(&caps), sizeof(caps));
      if (result != MMSYSERR_NOERROR) {
        LOG(ERROR) << "midiInGetDevCaps fails on device " << device_id;
        continue;
      }
      ports.push_back(base::MakeUnique<InPort>(device_id, caps));
    }
    return ports;
  }

  void NotifyPortStateSet(DynamicallyInitializedMidiManagerWin* manager) {
    manager->PostReplyTask(
        base::Bind(&DynamicallyInitializedMidiManagerWin::SetInputPortState,
                   base::Unretained(manager), index_, info_.state));
  }

  void NotifyPortAdded(DynamicallyInitializedMidiManagerWin* manager) {
    manager->PostReplyTask(
        base::Bind(&DynamicallyInitializedMidiManagerWin::AddInputPort,
                   base::Unretained(manager), info_));
  }
};

// TODO(toyoshim): Following patches will implement actual functions.
class DynamicallyInitializedMidiManagerWin::OutPort final : public Port {
 public:
  OutPort(UINT device_id, const MIDIOUTCAPS2W& caps)
      : Port("output",
             device_id,
             caps.wMid,
             caps.wPid,
             caps.vDriverVersion,
             base::WideToUTF8(
                 base::string16(caps.szPname, wcslen(caps.szPname)))),
        software_(caps.wTechnology == MOD_SWSYNTH) {}

  static std::vector<std::unique_ptr<OutPort>> EnumerateActivePorts() {
    std::vector<std::unique_ptr<OutPort>> ports;
    const UINT num_devices = midiOutGetNumDevs();
    for (UINT device_id = 0; device_id < num_devices; ++device_id) {
      MIDIOUTCAPS2W caps;
      MMRESULT result = midiOutGetDevCaps(
          device_id, reinterpret_cast<LPMIDIOUTCAPSW>(&caps), sizeof(caps));
      if (result != MMSYSERR_NOERROR) {
        LOG(ERROR) << "midiOutGetDevCaps fails on device " << device_id;
        continue;
      }
      ports.push_back(base::MakeUnique<OutPort>(device_id, caps));
    }
    return ports;
  }

  // Port overrides:
  bool Connect() override {
    // Until |software| option is supported, disable Microsoft GS Wavetable
    // Synth that has a known security issue.
    if (software_ && manufacturer_id_ == MM_MICROSOFT &&
        (product_id_ == MM_MSFT_WDMAUDIO_MIDIOUT ||
         product_id_ == MM_MSFT_GENERIC_MIDISYNTH)) {
      return false;
    }
    return Port::Connect();
  }

  // Port Overrides:
  void NotifyPortStateSet(DynamicallyInitializedMidiManagerWin* manager) {
    manager->PostReplyTask(
        base::Bind(&DynamicallyInitializedMidiManagerWin::SetOutputPortState,
                   base::Unretained(manager), index_, info_.state));
  }

  void NotifyPortAdded(DynamicallyInitializedMidiManagerWin* manager) {
    manager->PostReplyTask(
        base::Bind(&DynamicallyInitializedMidiManagerWin::AddOutputPort,
                   base::Unretained(manager), info_));
  }

  const bool software_;
};

DynamicallyInitializedMidiManagerWin::DynamicallyInitializedMidiManagerWin(
    MidiService* service)
    : MidiManager(service), instance_id_(IssueNextInstanceId()) {
  base::AutoLock lock(*GetInstanceIdLock());
  CHECK_EQ(kInvalidInstanceId, g_active_instance_id);

  // Obtains the task runner for the current thread that hosts this instnace.
  thread_runner_ = base::ThreadTaskRunnerHandle::Get();
}

DynamicallyInitializedMidiManagerWin::~DynamicallyInitializedMidiManagerWin() {
  base::AutoLock lock(*GetInstanceIdLock());
  CHECK_EQ(kInvalidInstanceId, g_active_instance_id);
  CHECK(thread_runner_->BelongsToCurrentThread());
}

void DynamicallyInitializedMidiManagerWin::PostReplyTask(
    const base::Closure& task) {
  thread_runner_->PostTask(FROM_HERE, base::Bind(&RunTask, instance_id_, task));
}

void DynamicallyInitializedMidiManagerWin::StartInitialization() {
  {
    base::AutoLock lock(*GetInstanceIdLock());
    CHECK_EQ(kInvalidInstanceId, g_active_instance_id);
    g_active_instance_id = instance_id_;
    CHECK_EQ(nullptr, g_manager_instance);
    g_manager_instance = this;
  }
  // Registers on the I/O thread to be notified on the I/O thread.
  CHECK(thread_runner_->BelongsToCurrentThread());
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);

  // Starts asynchronous initialization on TaskRunner.
  PostTask(
      base::Bind(&DynamicallyInitializedMidiManagerWin::InitializeOnTaskRunner,
                 base::Unretained(this)));
}

void DynamicallyInitializedMidiManagerWin::Finalize() {
  // Unregisters on the I/O thread. OnDevicesChanged() won't be called any more.
  CHECK(thread_runner_->BelongsToCurrentThread());
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
  {
    base::AutoLock lock(*GetInstanceIdLock());
    CHECK_EQ(instance_id_, g_active_instance_id);
    g_active_instance_id = kInvalidInstanceId;
    CHECK_EQ(this, g_manager_instance);
    g_manager_instance = nullptr;
  }

  // Ensures that no task runs on TaskRunner so to destruct the instance safely.
  // Tasks that did not started yet will do nothing after invalidate the
  // instance ID above.
  base::AutoLock lock(*GetTaskLock());
}

void DynamicallyInitializedMidiManagerWin::DispatchSendMidiData(
    MidiManagerClient* client,
    uint32_t port_index,
    const std::vector<uint8_t>& data,
    double timestamp) {
  // TODO(toyoshim): Following patches will implement.
}

void DynamicallyInitializedMidiManagerWin::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  // Notified on the I/O thread.
  CHECK(thread_runner_->BelongsToCurrentThread());

  switch (device_type) {
    case base::SystemMonitor::DEVTYPE_AUDIO:
    case base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE:
      // Add case of other unrelated device types here.
      return;
    case base::SystemMonitor::DEVTYPE_UNKNOWN: {
      PostTask(base::Bind(
          &DynamicallyInitializedMidiManagerWin::UpdateDeviceListOnTaskRunner,
          base::Unretained(this)));
      break;
    }
  }
}

void DynamicallyInitializedMidiManagerWin::PostTask(const base::Closure& task) {
  service()
      ->GetTaskRunner(kTaskRunner)
      ->PostTask(FROM_HERE, base::Bind(&RunTask, instance_id_, task));
}

void DynamicallyInitializedMidiManagerWin::InitializeOnTaskRunner() {
  UpdateDeviceListOnTaskRunner();
  PostReplyTask(
      base::Bind(&DynamicallyInitializedMidiManagerWin::CompleteInitialization,
                 base::Unretained(this), mojom::Result::OK));
}

void DynamicallyInitializedMidiManagerWin::UpdateDeviceListOnTaskRunner() {
  std::vector<std::unique_ptr<InPort>> active_input_ports =
      InPort::EnumerateActivePorts();
  ReflectActiveDeviceList(this, &input_ports_, &active_input_ports);

  std::vector<std::unique_ptr<OutPort>> active_output_ports =
      OutPort::EnumerateActivePorts();
  ReflectActiveDeviceList(this, &output_ports_, &active_output_ports);

  // TODO(toyoshim): This method may run before internal MIDI device lists that
  // Windows manages were updated. This may be because MIDI driver may be loaded
  // after the raw device list was updated. To avoid this problem, we may want
  // to retry device check later if no changes are detected here.
}

template <typename T>
void DynamicallyInitializedMidiManagerWin::ReflectActiveDeviceList(
    DynamicallyInitializedMidiManagerWin* manager,
    std::vector<T>* known_ports,
    std::vector<T>* active_ports) {
  // Update existing port states.
  for (const auto& port : *known_ports) {
    const auto& it = std::find_if(
        active_ports->begin(), active_ports->end(),
        [&port](const auto& candidate) { return *candidate == *port; });
    if (it == active_ports->end()) {
      if (port->Disconnect())
        port->NotifyPortStateSet(this);
    } else {
      port->set_device_id((*it)->device_id());
      if (port->Connect())
        port->NotifyPortStateSet(this);
    }
  }

  // Find new ports from active ports and append them to known ports.
  for (auto& port : *active_ports) {
    if (std::find_if(known_ports->begin(), known_ports->end(),
                     [&port](const auto& candidate) {
                       return *candidate == *port;
                     }) == known_ports->end()) {
      size_t index = known_ports->size();
      port->set_index(index);
      known_ports->push_back(std::move(port));
      (*known_ports)[index]->Connect();
      (*known_ports)[index]->NotifyPortAdded(this);
    }
  }
}

}  // namespace midi
