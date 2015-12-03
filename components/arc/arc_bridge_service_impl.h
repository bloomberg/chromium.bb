// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_
#define COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "components/arc/arc_bridge_service.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace arc {

// Real IPC based ArcBridgeService that is used in production.
class ArcBridgeServiceImpl : public ArcBridgeService,
                             public IPC::Listener {
 public:
  ArcBridgeServiceImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& file_task_runner);
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

 private:
  friend class ArcBridgeTest;
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ArcBridgeTest, ShutdownMidStartup);

  // If all pre-requisites are true (ARC is available, it has been enabled, and
  // the session has started), and ARC is stopped, start ARC. If ARC is running
  // and the pre-requisites stop being true, stop ARC.
  void PrerequisitesChanged();

  // Binds to the socket specified by |socket_path|.
  void SocketConnect(const base::FilePath& socket_path);

  // Binds to the socket specified by |socket_path| after creating its parent
  // directory is present.
  void SocketConnectAfterEnsureParentDirectory(
      const base::FilePath& socket_path,
      bool directory_present);

  // Internal connection method. Separated to make testing easier.
  bool Connect(const IPC::ChannelHandle& handle, IPC::Channel::Mode mode);

  // Finishes connecting after setting socket permissions.
  void SocketConnectAfterSetSocketPermissions(const base::FilePath& socket_path,
                                              bool socket_permissions_success);

  // Stops the running instance.
  void StopInstance();

  // Called when the instance has reached a boot phase
  void OnInstanceBootPhase(InstanceBootPhase phase);
  // Handler for ArcInstanceHostMsg_NotificationPosted message.
  void OnNotificationPostedFromAndroid(const ArcNotificationData& data);
  // Handler for ArcInstanceHostMsg_NotificationRemoved message.
  void OnNotificationRemovedFromAndroid(const std::string& key);

  // Called whenever ARC sends information about available apps.
  void OnAppListRefreshed(const std::vector<arc::AppInfo>& apps);

  // Called whenever ARC sends app icon data for specific scale factor.
  void OnAppIcon(const std::string& package,
                 const std::string& activity,
                 ScaleFactor scale_factor,
                 const std::vector<uint8_t>& icon_png_data);

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override;

  // DBus callbacks.
  void OnArcAvailable(bool available);
  void OnInstanceStarted(bool success);
  void OnInstanceStopped(bool success);

  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  scoped_ptr<IPC::ChannelProxy> ipc_channel_;

  // If the user's session has started.
  bool session_started_;

  // WeakPtrFactory to use callbacks.
  base::WeakPtrFactory<ArcBridgeServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeServiceImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_BRIDGE_SERVICE_IMPL_H_
