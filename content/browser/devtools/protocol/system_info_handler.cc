// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/system_info_handler.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_switches.h"

namespace content {
namespace protocol {

namespace {

using SystemInfo::GPUDevice;
using SystemInfo::GPUInfo;
using GetInfoCallback = SystemInfo::Backend::GetInfoCallback;

// Give the GPU process a few seconds to provide GPU info.
const int kGPUInfoWatchdogTimeoutMs = 5000;

class AuxGPUInfoEnumerator : public gpu::GPUInfo::Enumerator {
 public:
  AuxGPUInfoEnumerator(protocol::DictionaryValue* dictionary)
      : dictionary_(dictionary),
        in_aux_attributes_(false) { }

  void AddInt64(const char* name, int64_t value) override {
    if (in_aux_attributes_)
      dictionary_->setDouble(name, value);
  }

  void AddInt(const char* name, int value) override {
    if (in_aux_attributes_)
      dictionary_->setInteger(name, value);
  }

  void AddString(const char* name, const std::string& value) override {
    if (in_aux_attributes_)
      dictionary_->setString(name, value);
  }

  void AddBool(const char* name, bool value) override {
    if (in_aux_attributes_)
      dictionary_->setBoolean(name, value);
  }

  void AddTimeDeltaInSecondsF(const char* name,
                              const base::TimeDelta& value) override {
    if (in_aux_attributes_)
      dictionary_->setDouble(name, value.InSecondsF());
  }

  void BeginGPUDevice() override {}

  void EndGPUDevice() override {}

  void BeginVideoDecodeAcceleratorSupportedProfile() override {}

  void EndVideoDecodeAcceleratorSupportedProfile() override {}

  void BeginVideoEncodeAcceleratorSupportedProfile() override {}

  void EndVideoEncodeAcceleratorSupportedProfile() override {}

  void BeginAuxAttributes() override {
    in_aux_attributes_ = true;
  }

  void EndAuxAttributes() override {
    in_aux_attributes_ = false;
  }

 private:
  protocol::DictionaryValue* dictionary_;
  bool in_aux_attributes_;
};

std::unique_ptr<GPUDevice> GPUDeviceToProtocol(
    const gpu::GPUInfo::GPUDevice& device) {
  return GPUDevice::Create().SetVendorId(device.vendor_id)
                            .SetDeviceId(device.device_id)
                            .SetVendorString(device.vendor_string)
                            .SetDeviceString(device.device_string)
                            .Build();
}

void SendGetInfoResponse(std::unique_ptr<GetInfoCallback> callback) {
  gpu::GPUInfo gpu_info = GpuDataManager::GetInstance()->GetGPUInfo();
  std::unique_ptr<protocol::Array<GPUDevice>> devices =
      protocol::Array<GPUDevice>::create();
  devices->addItem(GPUDeviceToProtocol(gpu_info.gpu));
  for (const auto& device : gpu_info.secondary_gpus)
    devices->addItem(GPUDeviceToProtocol(device));

  std::unique_ptr<protocol::DictionaryValue> aux_attributes =
      protocol::DictionaryValue::create();
  AuxGPUInfoEnumerator enumerator(aux_attributes.get());
  gpu_info.EnumerateFields(&enumerator);

  std::unique_ptr<base::DictionaryValue> base_feature_status =
      base::WrapUnique(GetFeatureStatus());
  std::unique_ptr<protocol::DictionaryValue> feature_status =
      protocol::DictionaryValue::cast(
          protocol::toProtocolValue(base_feature_status.get(), 1000));

  std::unique_ptr<protocol::Array<std::string>> driver_bug_workarounds =
      protocol::Array<std::string>::create();
  for (const std::string& s : GetDriverBugWorkarounds())
      driver_bug_workarounds->addItem(s);

  std::unique_ptr<GPUInfo> gpu = GPUInfo::Create()
      .SetDevices(std::move(devices))
      .SetAuxAttributes(std::move(aux_attributes))
      .SetFeatureStatus(std::move(feature_status))
      .SetDriverBugWorkarounds(std::move(driver_bug_workarounds))
      .Build();

  callback->sendSuccess(std::move(gpu), gpu_info.machine_model_name,
      gpu_info.machine_model_version);
}

}  // namespace

class SystemInfoHandlerGpuObserver : public content::GpuDataManagerObserver {
 public:
  explicit SystemInfoHandlerGpuObserver(
      std::unique_ptr<GetInfoCallback> callback)
      : callback_(std::move(callback)),
        weak_factory_(this) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SystemInfoHandlerGpuObserver::ObserverWatchdogCallback,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kGPUInfoWatchdogTimeoutMs));

    GpuDataManager::GetInstance()->AddObserver(this);
    // There's no other method available to request just essential GPU info.
    GpuDataManager::GetInstance()->RequestCompleteGpuInfoIfNeeded();
  }

  void OnGpuInfoUpdate() override {
    UnregisterAndSendResponse();
  }

  void OnGpuProcessCrashed(base::TerminationStatus exit_code) override {
    UnregisterAndSendResponse();
  }

  void ObserverWatchdogCallback() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    UnregisterAndSendResponse();
  }

  void UnregisterAndSendResponse() {
    GpuDataManager::GetInstance()->RemoveObserver(this);
    SendGetInfoResponse(std::move(callback_));
    delete this;
  }

 private:
  std::unique_ptr<GetInfoCallback> callback_;
  base::WeakPtrFactory<SystemInfoHandlerGpuObserver> weak_factory_;
};

SystemInfoHandler::SystemInfoHandler()
    : DevToolsDomainHandler(SystemInfo::Metainfo::domainName) {
}

SystemInfoHandler::~SystemInfoHandler() {
}

void SystemInfoHandler::Wire(UberDispatcher* dispatcher) {
  SystemInfo::Dispatcher::wire(dispatcher, this);
}

void SystemInfoHandler::GetInfo(
    std::unique_ptr<GetInfoCallback> callback) {
  std::string reason;
  if (!GpuDataManager::GetInstance()->GpuAccessAllowed(&reason) ||
      GpuDataManager::GetInstance()->IsEssentialGpuInfoAvailable() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGpuTestingNoCompleteInfoCollection)) {
    // The GpuDataManager already has all of the information needed to make
    // GPU-based blacklisting decisions. Post a task to give it to the
    // client asynchronously.
    //
    // Waiting for complete GPU info in the if-test above seems to
    // frequently hit internal timeouts in the launching of the unsandboxed
    // GPU process in debug builds on Windows.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&SendGetInfoResponse, base::Passed(std::move(callback))));
  } else {
    // We will be able to get more information from the GpuDataManager.
    // Register a transient observer with it to call us back when the
    // information is available.
    new SystemInfoHandlerGpuObserver(std::move(callback));
  }
}

}  // namespace protocol
}  // namespace content
