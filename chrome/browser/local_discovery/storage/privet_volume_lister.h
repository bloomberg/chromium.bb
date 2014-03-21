// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_VOLUME_LISTER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_VOLUME_LISTER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"

#if defined(ENABLE_SERVICE_DISCOVERY)
#include "chrome/browser/local_discovery/privet_device_lister.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#endif

namespace local_discovery {

// This class will eventually list all of the user's Privet storage devices,
// but during prototyping phase searches the local network for Privet storage
// devices.
#if defined(ENABLE_SERVICE_DISCOVERY)
class PrivetVolumeLister : public PrivetDeviceLister::Delegate {
#else
class PrivetVolumeLister {
#endif
 public:
  struct VolumeInfo {
    std::string volume_id;
    std::string volume_label;
    base::FilePath volume_path;
  };

  typedef std::vector<VolumeInfo> VolumeList;
  typedef base::Callback<void(const VolumeList&)> ResultCallback;

  explicit PrivetVolumeLister(const ResultCallback& callback);
  virtual ~PrivetVolumeLister();

  void Start();

  const std::vector<VolumeInfo>& volume_list() const {
    return canonical_volume_list_;
  }

#if defined(ENABLE_SERVICE_DISCOVERY)
  virtual void DeviceChanged(bool added,
                             const std::string& name,
                             const DeviceDescription& description) OVERRIDE;
  virtual void DeviceRemoved(const std::string& name) OVERRIDE;
  virtual void DeviceCacheFlushed() OVERRIDE;
#endif

 private:
#if defined(ENABLE_SERVICE_DISCOVERY)
  void FinishSearch();

  scoped_refptr<ServiceDiscoverySharedClient> service_discovery_client_;
  scoped_ptr<PrivetDeviceLister> privet_lister_;
#endif

  std::vector<VolumeInfo> available_volumes_;
  std::vector<VolumeInfo> canonical_volume_list_;
  ResultCallback callback_;
  base::WeakPtrFactory<PrivetVolumeLister> weak_factory_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_VOLUME_LISTER_H_
