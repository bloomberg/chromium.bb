// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/system_info_handler.h"

#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "gpu/config/gpu_info.h"

namespace content {
namespace devtools {
namespace system_info {

namespace {

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

  void BeginAuxAttributes() override {
    in_aux_attributes_ = true;
  }

  void EndAuxAttributes() override {
    in_aux_attributes_ = false;
  }

 private:
  base::DictionaryValue* dictionary_;
  bool in_aux_attributes_;
};

scoped_refptr<GPUDevice> GPUDeviceToProtocol(
    const gpu::GPUInfo::GPUDevice& device) {
  return GPUDevice::Create()->set_vendor_id(device.vendor_id)
                            ->set_device_id(device.device_id)
                            ->set_vendor_string(device.vendor_string)
                            ->set_device_string(device.device_string);
}

}  // namespace

typedef DevToolsProtocolClient::Response Response;

SystemInfoHandler::SystemInfoHandler() {
}

SystemInfoHandler::~SystemInfoHandler() {
}

Response SystemInfoHandler::GetInfo(
    scoped_refptr<devtools::system_info::GPUInfo>* gpu,
    std::string* model_name,
    std::string* model_version) {
  gpu::GPUInfo gpu_info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();

  std::vector<scoped_refptr<GPUDevice>> devices;
  devices.push_back(GPUDeviceToProtocol(gpu_info.gpu));
  for (const auto& device : gpu_info.secondary_gpus)
    devices.push_back(GPUDeviceToProtocol(device));

  scoped_ptr<base::DictionaryValue> aux_attributes(new base::DictionaryValue);
  AuxGPUInfoEnumerator enumerator(aux_attributes.get());
  gpu_info.EnumerateFields(&enumerator);

  *model_name = gpu_info.machine_model_name;
  *model_version = gpu_info.machine_model_version;
  *gpu = GPUInfo::Create()
      ->set_devices(devices)
      ->set_aux_attributes(aux_attributes.Pass())
      ->set_feature_status(make_scoped_ptr(GetFeatureStatus()))
      ->set_driver_bug_workarounds(GetDriverBugWorkarounds());
  return Response::OK();
}

}  // namespace system_info
}  // namespace devtools
}  // namespace content
