// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_GPU_ARC_VIDEO_SERVICE_H_
#define CHROME_GPU_GPU_ARC_VIDEO_SERVICE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "components/arc/common/video.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace chromeos {
namespace arc {

// GpuArcVideoService manages life-cycle and IPC message translation for
// ArcVideoAccelerator.
//
// For each creation request from GpuArcVideoServiceHost, GpuArcVideoService
// will create a new IPC channel.
class GpuArcVideoService : public ::arc::VideoHost {
 public:
  class AcceleratorStub;

  // |request| is mojo interface request of arc::VideoHost.
  explicit GpuArcVideoService(mojo::InterfaceRequest<::arc::VideoHost> request);

  // Upon deletion, all ArcVideoAccelerator will be deleted and the associated
  // IPC channels are closed.
  ~GpuArcVideoService() override;

  // Removes the reference of |stub| (and trigger deletion) from this class.
  void RemoveClient(AcceleratorStub* stub);

 private:
  // arc::VideoHost implementation.
  void OnRequestArcVideoAcceleratorChannel(
      const OnRequestArcVideoAcceleratorChannelCallback& callback) override;

  base::ThreadChecker thread_checker_;

  // Binding of arc::VideoHost. It also takes ownership of |this|.
  mojo::StrongBinding<::arc::VideoHost> binding_;

  // Bookkeeping all accelerator stubs.
  std::map<AcceleratorStub*, std::unique_ptr<AcceleratorStub>>
      accelerator_stubs_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcVideoService);
};

}  // namespace arc
}  // namespace chromeos

#endif  // CHROME_GPU_GPU_ARC_VIDEO_SERVICE_H_
