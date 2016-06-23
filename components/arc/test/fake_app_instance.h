// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "components/arc/common/app.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class FakeAppInstance : public mojom::AppInstance {
 public:
  class Request {
   public:
    Request(const std::string& package_name, const std::string& activity)
        : package_name_(package_name), activity_(activity) {}
    ~Request() {}

    const std::string& package_name() const { return package_name_; }

    const std::string& activity() const { return activity_; }

    bool IsForApp(const mojom::AppInfo& app_info) const {
      return package_name_ == app_info.package_name &&
             activity_ == app_info.activity;
    }

   private:
    std::string package_name_;
    std::string activity_;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  class IconRequest : public Request {
   public:
    IconRequest(const std::string& package_name,
                const std::string& activity,
                mojom::ScaleFactor scale_factor)
        : Request(package_name, activity),
          scale_factor_(static_cast<int>(scale_factor)) {}
    ~IconRequest() {}

    int scale_factor() const { return scale_factor_; }

   private:
    int scale_factor_;

    DISALLOW_COPY_AND_ASSIGN(IconRequest);
  };

  class ShortcutIconRequest {
   public:
    ShortcutIconRequest(const std::string& icon_resource_id,
                        mojom::ScaleFactor scale_factor)
        : icon_resource_id_(icon_resource_id),
          scale_factor_(static_cast<int>(scale_factor)) {}
    ~ShortcutIconRequest() {}

    const std::string& icon_resource_id() const { return icon_resource_id_; }
    int scale_factor() const { return scale_factor_; }

   private:
    std::string icon_resource_id_;
    int scale_factor_;

    DISALLOW_COPY_AND_ASSIGN(ShortcutIconRequest);
  };

  explicit FakeAppInstance(mojom::AppHost* app_host);
  ~FakeAppInstance() override;

  void Bind(mojo::InterfaceRequest<mojom::AppInstance> interface_request) {
    binding_.Bind(std::move(interface_request));
  }

  // mojom::AppInstance overrides:
  void Init(mojom::AppHostPtr host_ptr) override {}
  void RefreshAppList() override;
  void LaunchApp(const mojo::String& package_name,
                 const mojo::String& activity,
                 const gfx::Rect& dimension) override;
  void RequestAppIcon(const mojo::String& package_name,
                      const mojo::String& activity,
                      mojom::ScaleFactor scale_factor) override;
  void LaunchIntent(const mojo::String& intent_uri,
                    const gfx::Rect& dimension_on_screen) override;
  void RequestIcon(const mojo::String& icon_resource_id,
                   arc::mojom::ScaleFactor scale_factor,
                   const RequestIconCallback& callback) override;
  void RemoveCachedIcon(const mojo::String& icon_resource_id) override;
  void CanHandleResolution(
      const mojo::String& package_name,
      const mojo::String& activity,
      const gfx::Rect& dimension,
      const CanHandleResolutionCallback& callback) override;
  void UninstallPackage(const mojo::String& package_name) override;
  void GetTaskInfo(int32_t task_id,
                   const GetTaskInfoCallback& callback) override;
  void SetTaskActive(int32_t task_id) override;
  void CloseTask(int32_t task_id) override;
  void ShowPackageInfoDeprecated(const mojo::String& package_name,
                                 const gfx::Rect& dimension_on_screen) override;
  void ShowPackageInfoOnPage(const mojo::String& package_name,
                             mojom::ShowPackageInfoPage page,
                             const gfx::Rect& dimension_on_screen) override;
  void SetNotificationsEnabled(const mojo::String& package_name,
                               bool enabled) override;
  void InstallPackage(mojom::ArcPackageInfoPtr arcPackageInfo) override;

  // Methods to reply messages.
  void SendRefreshAppList(const std::vector<mojom::AppInfo>& apps);
  void SendTaskCreated(int32_t taskId, const mojom::AppInfo& app);
  void SendTaskDestroyed(int32_t taskId);
  bool GenerateAndSendIcon(const mojom::AppInfo& app,
                           mojom::ScaleFactor scale_factor,
                           std::string* png_data_as_string);
  void SendInstallShortcut(const mojom::ShortcutInfo& shortcuts);
  void SendInstallShortcuts(const std::vector<mojom::ShortcutInfo>& shortcuts);
  void SetTaskInfo(int32_t task_id,
                   const std::string& package_name,
                   const std::string& activity);
  void SendRefreshPackageList(
      const std::vector<mojom::ArcPackageInfo>& packages);
  void SendPackageAdded(const mojom::ArcPackageInfo& package);
  void SendPackageUninstalled(const mojo::String& pacakge_name);

  int refresh_app_list_count() const { return refresh_app_list_count_; }

  const ScopedVector<Request>& launch_requests() const {
    return launch_requests_;
  }

  const ScopedVector<mojo::String>& launch_intents() const {
    return launch_intents_;
  }

  const ScopedVector<IconRequest>& icon_requests() const {
    return icon_requests_;
  }

  const ScopedVector<ShortcutIconRequest>& shortcut_icon_requests() const {
    return shortcut_icon_requests_;
  }

  // This method can be called on tests when a method is intended to
  // be called across a Mojo proxy.
  void WaitForIncomingMethodCall();

  // As part of the initialization process, the instance side calls
  // mojom::AppHost::OnAppInstanceReady(), which in turn calls
  // mojom::AppInstance::Init() and
  // mojom::AppInstance::RefreshAppList(). This method should be called after a
  // call
  // to mojom::ArcBridgeHost::OnAppInstanceReady() to make sure all method calls
  // have
  // been dispatched.
  void WaitForOnAppInstanceReady();

 private:
  using TaskIdToInfo = std::map<int32_t, std::unique_ptr<Request>>;
  // Mojo endpoints.
  mojo::Binding<mojom::AppInstance> binding_;
  mojom::AppHost* app_host_;
  // Number of RefreshAppList calls.
  int refresh_app_list_count_ = 0;
  // Keeps information about launch requests.
  ScopedVector<Request> launch_requests_;
  // Keeps information about launch intents.
  ScopedVector<mojo::String> launch_intents_;
  // Keeps information about icon load requests.
  ScopedVector<IconRequest> icon_requests_;
  // Keeps information about shortcut icon load requests.
  ScopedVector<ShortcutIconRequest> shortcut_icon_requests_;
  // Keeps information for running tasks.
  TaskIdToInfo task_id_to_info_;

  bool GetFakeIcon(mojom::ScaleFactor scale_factor,
                   std::string* png_data_as_string);

  DISALLOW_COPY_AND_ASSIGN(FakeAppInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_
