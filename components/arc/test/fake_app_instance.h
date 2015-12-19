// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_vector.h"
#include "components/arc/common/app.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class FakeAppInstance : public AppInstance {
 public:
  class Request {
   public:
    Request(const std::string& package, const std::string& activity)
        : package_(package), activity_(activity) {}
    ~Request() {}

    const std::string& package() const { return package_; }

    const std::string& activity() const { return activity_; }

    bool IsForApp(const AppInfo& app_info) const {
      return package_ == app_info.package && activity_ == app_info.activity;
    }

   private:
    std::string package_;
    std::string activity_;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  class IconRequest : public Request {
   public:
    IconRequest(const std::string& package,
                const std::string& activity,
                ScaleFactor scale_factor)
        : Request(package, activity), scale_factor_(scale_factor) {}
    ~IconRequest() {}

    int scale_factor() const { return scale_factor_; }

   private:
    int scale_factor_;

    DISALLOW_COPY_AND_ASSIGN(IconRequest);
  };

  explicit FakeAppInstance(AppHost* app_host);
  ~FakeAppInstance() override;

  void Bind(mojo::InterfaceRequest<AppInstance> interface_request) {
    binding_.Bind(std::move(interface_request));
  }

  // AppInstance overrides:
  void Init(AppHostPtr host_ptr) override {}
  void RefreshAppList() override;
  void LaunchApp(const mojo::String& package,
                 const mojo::String& activity) override;
  void RequestAppIcon(const mojo::String& package,
                      const mojo::String& activity,
                      ScaleFactor scale_factor) override;

  // Methods to reply messages.
  void SendRefreshAppList(const std::vector<AppInfo>& apps);
  bool GenerateAndSendIcon(const AppInfo& app,
                           ScaleFactor scale_factor,
                           std::string* png_data_as_string);

  int refresh_app_list_count() const { return refresh_app_list_count_; }

  const ScopedVector<Request>& launch_requests() const {
    return launch_requests_;
  }

  const ScopedVector<IconRequest>& icon_requests() const {
    return icon_requests_;
  }

 private:
  // Mojo endpoints.
  mojo::Binding<AppInstance> binding_;
  AppHost* app_host_;
  // Number of RefreshAppList calls.
  int refresh_app_list_count_ = 0;
  // Keeps information about launch requests.
  ScopedVector<Request> launch_requests_;
  // Keeps information about icon load requests.
  ScopedVector<IconRequest> icon_requests_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_APP_INSTANCE_H_
