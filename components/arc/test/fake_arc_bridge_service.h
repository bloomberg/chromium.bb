// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_SERVICE_H_
#define COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_SERVICE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

class FakeArcBridgeService : public ArcBridgeService {
 public:
  class Request {
   public:
    Request(const std::string& package, const std::string& activity)
        : package_(package),
          activity_(activity) {
    }

    ~Request() {
    }

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
        : Request(package, activity),
          scale_factor_(scale_factor) {
    }

    ~IconRequest() {
    }

    int scale_factor() const { return scale_factor_; }

   private:
    int scale_factor_;

    DISALLOW_COPY_AND_ASSIGN(IconRequest);
  };

  FakeArcBridgeService();
  ~FakeArcBridgeService() override;

  // arc::ArcBridgeService
  void DetectAvailability() override;
  void HandleStartup() override;
  void Shutdown() override;
  bool RegisterInputDevice(const std::string& name,
                           const std::string& device_type,
                           base::ScopedFD fd) override;
  bool SendBroadcast(const std::string& action,
                     const std::string& package,
                     const std::string& clazz,
                     const base::DictionaryValue& extras) override;
  bool RefreshAppList() override;
  bool LaunchApp(const std::string& package,
                 const std::string& activity) override;
  bool RequestAppIcon(const std::string& package,
                      const std::string& activity,
                      ScaleFactor scale_factor) override;
  bool SendNotificationEventToAndroid(const std::string& key,
                                      ArcNotificationEvent event) override;
  bool RequestProcessList() override;

  int refresh_app_list_count() const { return refresh_app_list_count_; }

  const ScopedVector<Request>& launch_requests() const {
    return launch_requests_;
  }

  const ScopedVector<IconRequest>& icon_requests() const {
    return icon_requests_;
  }

  void SetReady();

  void SetStopped();

  bool HasObserver(const Observer* observer);
  bool HasAppObserver(const AppObserver* observer);

  void SendRefreshAppList(const std::vector<AppInfo>& apps);

  bool GenerateAndSendIcon(const AppInfo& app,
                           ScaleFactor scale_factor,
                           std::string* png_data);

 private:
  // Number of RefreshAppList calls.
  int refresh_app_list_count_ = 0;
  // Keeps information about launch requests.
  ScopedVector<Request> launch_requests_;
  // Keeps information about icon load requests.
  ScopedVector<IconRequest> icon_requests_;

  DISALLOW_COPY_AND_ASSIGN(FakeArcBridgeService);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_ARC_BRIDGE_SERVICE_H_
