// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/storage/privet_volume_lister.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "chrome/browser/local_discovery/storage/privet_filesystem_constants.h"

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/local_discovery/privet_device_lister_impl.h"
#endif

namespace local_discovery {

namespace {

#if defined(ENABLE_SERVICE_DISCOVERY)
const int kVolumeSearchDurationMs = 10000;

std::string EscapeSlashes(const std::string& str) {
  std::string output = "";
  for (size_t i = 0; i < str.length(); i++) {
    switch (str[i]) {
      case '/':
        output += "$s";
        break;
      case '\\':
        output += "$b";
        break;
      case '$':
        output += "$$";
        break;
      default:
        output += str[i];
    }
  }

  return output;
}

std::string RemoveSlashes(const std::string& str) {
    std::string output = "";
  for (size_t i = 0; i < str.length(); i++) {
    switch (str[i]) {
      case '/':
      case '\\':
        break;
      default:
        output += str[i];
    }
  }

  return output;
}
#endif  // ENABLE_SERVICE_DISCOVERY

}  // namespace

PrivetVolumeLister::PrivetVolumeLister(const ResultCallback& callback)
    : callback_(callback), weak_factory_(this) {
}

PrivetVolumeLister::~PrivetVolumeLister() {
}

void PrivetVolumeLister::Start() {
#if defined(ENABLE_SERVICE_DISCOVERY)
  service_discovery_client_ = ServiceDiscoverySharedClient::GetInstance();
  privet_lister_.reset(new PrivetDeviceListerImpl(service_discovery_client_,
                                                  this));
  privet_lister_->Start();
  privet_lister_->DiscoverNewDevices(false);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&PrivetVolumeLister::FinishSearch,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kVolumeSearchDurationMs));
#else
  callback_.Run(std::vector<VolumeInfo>());
#endif
}

#if defined(ENABLE_SERVICE_DISCOVERY)
void PrivetVolumeLister::DeviceChanged(bool added,
                                       const std::string& name,
                                       const DeviceDescription& description) {
  if (added && description.type == kPrivetTypeStorage) {
    VolumeInfo volume_info;
    volume_info.volume_id = name;
    volume_info.volume_label = description.name;
    base::FilePath mount_root = base::FilePath(
        local_discovery::kPrivetFilePath);

    // HACK(noamsml): Add extra dummy folder with the volume label so that the
    // file browser displays the correct volume label.

    std::string volume_id = EscapeSlashes(volume_info.volume_id);
    std::string volume_label = RemoveSlashes(volume_info.volume_label);

    volume_info.volume_path =
        mount_root.AppendASCII(volume_id).AppendASCII(volume_label);
    available_volumes_.push_back(volume_info);
  }
}

void PrivetVolumeLister::DeviceRemoved(const std::string& name) {
}

void PrivetVolumeLister::DeviceCacheFlushed() {
}

void PrivetVolumeLister::FinishSearch() {
  privet_lister_.reset();
  service_discovery_client_ = NULL;
  available_volumes_.swap(canonical_volume_list_);
  callback_.Run(canonical_volume_list_);
}
#endif

}  // namespace local_discovery
