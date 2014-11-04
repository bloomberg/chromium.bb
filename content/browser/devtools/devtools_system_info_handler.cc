// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_system_info_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "gpu/config/gpu_info.h"

namespace content {

namespace {

const char kAuxAttributes[] = "auxAttributes";
const char kDeviceId[] = "deviceId";
const char kDeviceString[] = "deviceString";
const char kDevices[] = "devices";
const char kDriverBugWorkarounds[] = "driverBugWorkarounds";
const char kFeatureStatus[] = "featureStatus";
const char kGPU[] = "gpu";
const char kModelName[] = "modelName";
const char kModelVersion[] = "modelVersion";
const char kVendorId[] = "vendorId";
const char kVendorString[] = "vendorString";

class AuxGPUInfoEnumerator : public gpu::GPUInfo::Enumerator {
 public:
  AuxGPUInfoEnumerator(base::DictionaryValue* dictionary)
      : dictionary_(dictionary),
        in_aux_attributes_(false) { }

  void AddInt64(const char* name, int64 value) override {
    if (in_aux_attributes_)
      dictionary_->SetDouble(name, value);
  }

  void AddInt(const char* name, int value) override {
    if (in_aux_attributes_)
      dictionary_->SetInteger(name, value);
  }

  void AddString(const char* name, const std::string& value) override {
    if (in_aux_attributes_)
      dictionary_->SetString(name, value);
  }

  void AddBool(const char* name, bool value) override {
    if (in_aux_attributes_)
      dictionary_->SetBoolean(name, value);
  }

  void AddTimeDeltaInSecondsF(const char* name,
                              const base::TimeDelta& value) override {
    if (in_aux_attributes_)
      dictionary_->SetDouble(name, value.InSecondsF());
  }

  void BeginGPUDevice() override {}

  void EndGPUDevice() override {}

  void BeginVideoEncodeAcceleratorSupportedProfile() override {}

  void EndVideoEncodeAcceleratorSupportedProfile() override {}

  void BeginAuxAttributes() override { in_aux_attributes_ = true; }

  void EndAuxAttributes() override { in_aux_attributes_ = false; }

 private:
  base::DictionaryValue* dictionary_;
  bool in_aux_attributes_;
};

base::DictionaryValue* GPUDeviceToDictionary(
    const gpu::GPUInfo::GPUDevice& device) {
  base::DictionaryValue* result = new base::DictionaryValue;
  result->SetInteger(kVendorId, device.vendor_id);
  result->SetInteger(kDeviceId, device.device_id);
  result->SetString(kVendorString, device.vendor_string);
  result->SetString(kDeviceString, device.device_string);
  return result;
}

}  // namespace

DevToolsSystemInfoHandler::DevToolsSystemInfoHandler() {
  RegisterCommandHandler(devtools::SystemInfo::getInfo::kName,
                         base::Bind(&DevToolsSystemInfoHandler::OnGetInfo,
                                    base::Unretained(this)));
}

DevToolsSystemInfoHandler::~DevToolsSystemInfoHandler() {
}

scoped_refptr<DevToolsProtocol::Response>
DevToolsSystemInfoHandler::OnGetInfo(
    scoped_refptr<DevToolsProtocol::Command> command) {
  gpu::GPUInfo gpu_info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  base::DictionaryValue* gpu_dict = new base::DictionaryValue;

  base::ListValue* devices = new base::ListValue;
  devices->Append(GPUDeviceToDictionary(gpu_info.gpu));
  for (size_t ii = 0; ii < gpu_info.secondary_gpus.size(); ++ii) {
    devices->Append(GPUDeviceToDictionary(gpu_info.secondary_gpus[ii]));
  }
  gpu_dict->Set(kDevices, devices);

  base::DictionaryValue* aux_attributes = new base::DictionaryValue;
  AuxGPUInfoEnumerator enumerator(aux_attributes);
  gpu_info.EnumerateFields(&enumerator);
  gpu_dict->Set(kAuxAttributes, aux_attributes);

  gpu_dict->Set(kFeatureStatus,  GetFeatureStatus());

  gpu_dict->Set(kDriverBugWorkarounds, GetDriverBugWorkarounds());

  base::DictionaryValue* system_dict = new base::DictionaryValue;
  system_dict->SetString(kModelName, gpu_info.machine_model_name);
  system_dict->SetString(kModelVersion, gpu_info.machine_model_version);
  system_dict->Set(kGPU, gpu_dict);
  return command->SuccessResponse(system_dict);
}

}  // namespace content
