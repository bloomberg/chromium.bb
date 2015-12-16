// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_

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
                             public ArcBridgeHost,
                             public ArcBridgeBootstrap::Delegate {
 public:
  explicit ArcBridgeServiceImpl(scoped_ptr<ArcBridgeBootstrap> bootstrap);
  ~ArcBridgeServiceImpl() override;

  void DetectAvailability() override;

  void HandleStartup() override;

  void Shutdown() override;

  bool RegisterInputDevice(const std::string& name,
                           const std::string& device_type,
                           base::ScopedFD fd) override;

  bool SendNotificationEventToAndroid(const std::string& key,
                                      ArcNotificationEvent event) override;

  // Requests to refresh an app list.
  bool RefreshAppList() override;

  // Requests to launch an app.
  bool LaunchApp(const std::string& package,
                 const std::string& activity) override;

  // Requests to load an icon of specific scale_factor.
  bool RequestAppIcon(const std::string& package,
                      const std::string& activity,
                      ScaleFactor scale_factor) override;

  // Requests ARC process list.
  bool RequestProcessList() override;

 private:
  friend class ArcBridgeTest;
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Prerequisites);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, ShutdownMidStartup);

  // If all pre-requisites are true (ARC is available, it has been enabled, and
  // the session has started), and ARC is stopped, start ARC. If ARC is running
  // and the pre-requisites stop being true, stop ARC.
  void PrerequisitesChanged();

  // Stops the running instance.
  void StopInstance();

  // Called when the instance has reached a boot phase
  void OnInstanceBootPhase(InstanceBootPhase phase) override;
  // Handler for ArcInstanceHostMsg_NotificationPosted message.
  void OnNotificationPosted(ArcNotificationDataPtr data) override;
  // Handler for ArcInstanceHostMsg_NotificationRemoved message.
  void OnNotificationRemoved(const mojo::String& key) override;

  // Called whenever ARC sends information about available apps.
  void OnAppListRefreshed(mojo::Array<arc::AppInfoPtr> apps) override;

  // Called whenever ARC sends app icon data for specific scale factor.
  void OnAppIcon(const mojo::String& package,
                 const mojo::String& activity,
                 ScaleFactor scale_factor,
                 mojo::Array<uint8_t> icon_png_data) override;

  // Called when the latest process list is reported from ARC.
  void OnUpdateProcessList(
      mojo::Array<RunningAppProcessInfoPtr> processes_ptr) override;

  // Called when the instance requests wake lock services
  void OnAcquireDisplayWakeLock(DisplayWakeLockType type) override;
  void OnReleaseDisplayWakeLock(DisplayWakeLockType type) override;

  // ArcBridgeBootstrap::Delegate:
  void OnConnectionEstablished(ArcBridgeInstancePtr instance) override;
  void OnStopped() override;

  // DBus callbacks.
  void OnArcAvailable(bool available);

  scoped_ptr<ArcBridgeBootstrap> bootstrap_;

  // Mojo endpoints.
  mojo::Binding<ArcBridgeHost> binding_;
  ArcBridgeInstancePtr instance_ptr_;

  // If the user's session has started.
  bool session_started_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcBridgeServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeServiceImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_
