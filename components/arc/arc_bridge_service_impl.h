// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/arc/arc_bridge_bootstrap.h"
#include "components/arc/arc_bridge_service.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace arc {

// Real IPC based ArcBridgeService that is used in production.
class ArcBridgeServiceImpl : public ArcBridgeService,
                             public ArcBridgeBootstrap::Delegate,
                             public mojom::ArcBridgeHost {
 public:
  explicit ArcBridgeServiceImpl(std::unique_ptr<ArcBridgeBootstrap> bootstrap);
  ~ArcBridgeServiceImpl() override;

  void SetDetectedAvailability(bool available) override;

  void HandleStartup() override;

  void Shutdown() override;

  // Normally, reconnecting after connection shutdown happens after a short
  // delay. When testing, however, we'd like it to happen immediately to avoid
  // adding unnecessary delays.
  void DisableReconnectDelayForTesting();

  // ArcHost:
  void OnAppInstanceReady(mojom::AppInstancePtr app_ptr) override;
  void OnAudioInstanceReady(mojom::AudioInstancePtr audio_ptr) override;
  void OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) override;
  void OnBluetoothInstanceReady(
      mojom::BluetoothInstancePtr bluetooth_ptr) override;
  void OnClipboardInstanceReady(
      mojom::ClipboardInstancePtr clipboard_ptr) override;
  void OnCrashCollectorInstanceReady(
      mojom::CrashCollectorInstancePtr crash_collector_ptr) override;
  void OnFileSystemInstanceReady(
      mojom::FileSystemInstancePtr file_system_ptr) override;
  void OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) override;
  void OnIntentHelperInstanceReady(
      mojom::IntentHelperInstancePtr intent_helper_ptr) override;
  void OnMetricsInstanceReady(mojom::MetricsInstancePtr metrics_ptr) override;
  void OnNetInstanceReady(mojom::NetInstancePtr net_ptr) override;
  void OnNotificationsInstanceReady(
      mojom::NotificationsInstancePtr notifications_ptr) override;
  void OnObbMounterInstanceReady(
      mojom::ObbMounterInstancePtr obb_mounter_ptr) override;
  void OnPolicyInstanceReady(mojom::PolicyInstancePtr policy_ptr) override;
  void OnPowerInstanceReady(mojom::PowerInstancePtr power_ptr) override;
  void OnProcessInstanceReady(mojom::ProcessInstancePtr process_ptr) override;
  void OnStorageManagerInstanceReady(
      mojom::StorageManagerInstancePtr storage_manager_ptr) override;
  void OnVideoInstanceReady(mojom::VideoInstancePtr video_ptr) override;
  void OnWindowManagerInstanceReady(
      mojom::WindowManagerInstancePtr window_manager_ptr) override;

 private:
  friend class ArcBridgeTest;
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Restart);

  // If all pre-requisites are true (ARC is available, it has been enabled, and
  // the session has started), and ARC is stopped, start ARC. If ARC is running
  // and the pre-requisites stop being true, stop ARC.
  void PrerequisitesChanged();

  // Stops the running instance.
  void StopInstance();

  // ArcBridgeBootstrap::Delegate:
  void OnConnectionEstablished(mojom::ArcBridgeInstancePtr instance) override;
  void OnStopped() override;

  // Called when the bridge channel is closed. This typically only happens when
  // the ARC instance crashes. This is not called during shutdown.
  void OnChannelClosed();

  std::unique_ptr<ArcBridgeBootstrap> bootstrap_;

  // Mojo endpoints.
  mojo::Binding<mojom::ArcBridgeHost> binding_;
  mojom::ArcBridgeInstancePtr instance_ptr_;

  // If the user's session has started.
  bool session_started_;

  // If the instance had already been started but the connection to it was
  // lost. This should make the instance restart.
  bool reconnect_ = false;

  // Delay the reconnection.
  bool use_delay_before_reconnecting_ = true;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcBridgeServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeServiceImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_
