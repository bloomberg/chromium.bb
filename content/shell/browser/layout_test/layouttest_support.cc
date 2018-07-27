// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layouttest_support.h"

#include "build/build_config.h"
#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/public/browser/storage_partition.h"

#if defined(OS_MACOSX)
#include "content/browser/frame_host/popup_menu_helper_mac.h"
#include "gpu/ipc/service/image_transport_surface.h"
#endif

namespace content {

void EnableBrowserLayoutTestMode() {
#if defined(OS_MACOSX)
  gpu::ImageTransportSurface::SetAllowOSMesaForTesting(true);
  PopupMenuHelper::DontShowPopupMenuForTesting();
#endif
  RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
}

void TerminateAllSharedWorkersForTesting(StoragePartition* storage_partition,
                                         base::OnceClosure callback) {
  static_cast<SharedWorkerServiceImpl*>(
      storage_partition->GetSharedWorkerService())
      ->TerminateAllWorkersForTesting(std::move(callback));
}

void SetTestBluetoothScanDuration(BluetoothTestScanDurationSetting setting) {
  switch (setting) {
    case BluetoothTestScanDurationSetting::kImmediateTimeout:
      BluetoothDeviceChooserController::SetTestScanDurationForTesting(
          BluetoothDeviceChooserController::TestScanDurationSetting::
              IMMEDIATE_TIMEOUT);
      break;
    case BluetoothTestScanDurationSetting::kNeverTimeout:
      BluetoothDeviceChooserController::SetTestScanDurationForTesting(
          BluetoothDeviceChooserController::TestScanDurationSetting::
              NEVER_TIMEOUT);
      break;
  }
}

}  // namespace content
