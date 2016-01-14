// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAST_CONFIG_DELEGATE_MEDIA_ROUTER_H_
#define CHROME_BROWSER_UI_ASH_CAST_CONFIG_DELEGATE_MEDIA_ROUTER_H_

#include "ash/cast_config_delegate.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace media_router {
class MediaRouter;
}

class CastDeviceCache;

// A class which allows the ash tray to communicate with the media router.
class CastConfigDelegateMediaRouter : public ash::CastConfigDelegate {
 public:
  CastConfigDelegateMediaRouter();
  ~CastConfigDelegateMediaRouter() override;

  // The MediaRouter is not always enabled. We only use this backend when it is
  // enabled.
  static bool IsEnabled();
  static void SetMediaRouterForTest(media_router::MediaRouter* media_router);

 private:
  // CastConfigDelegate:
  bool HasCastExtension() const override;
  void RequestDeviceRefresh() override;
  void CastToReceiver(const std::string& receiver_id) override;
  void StopCasting(const std::string& route_id) override;
  bool HasOptions() const override;
  void LaunchCastOptions() override;
  void AddObserver(ash::CastConfigDelegate::Observer* observer) override;
  void RemoveObserver(ash::CastConfigDelegate::Observer* observer) override;

  // |devices_| stores the current source/route status that we query from.
  // This will return null until the media router is initialized.
  CastDeviceCache* devices();
  scoped_ptr<CastDeviceCache> devices_;

  base::ObserverList<ash::CastConfigDelegate::Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(CastConfigDelegateMediaRouter);
};

#endif  // CHROME_BROWSER_UI_ASH_CAST_CONFIG_DELEGATE_MEDIA_ROUTER_H_
