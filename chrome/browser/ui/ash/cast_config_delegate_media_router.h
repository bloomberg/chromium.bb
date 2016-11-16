// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAST_CONFIG_DELEGATE_MEDIA_ROUTER_H_
#define CHROME_BROWSER_UI_ASH_CAST_CONFIG_DELEGATE_MEDIA_ROUTER_H_

#include "ash/common/cast_config_delegate.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace media_router {
class MediaRouter;
}

class CastDeviceCache;

// A class which allows the ash tray to communicate with the media router.
class CastConfigDelegateMediaRouter : public ash::CastConfigDelegate,
                                      public content::NotificationObserver {
 public:
  CastConfigDelegateMediaRouter();
  ~CastConfigDelegateMediaRouter() override;

  static void SetMediaRouterForTest(media_router::MediaRouter* media_router);

 private:
  // CastConfigDelegate:
  void RequestDeviceRefresh() override;
  void CastToSink(const Sink& sink) override;
  void StopCasting(const Route& route) override;
  void AddObserver(ash::CastConfigDelegate::Observer* observer) override;
  void RemoveObserver(ash::CastConfigDelegate::Observer* observer) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // |devices_| stores the current source/route status that we query from.
  // This will return null until the media router is initialized.
  CastDeviceCache* devices();
  std::unique_ptr<CastDeviceCache> devices_;

  content::NotificationRegistrar registrar_;

  base::ObserverList<ash::CastConfigDelegate::Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(CastConfigDelegateMediaRouter);
};

#endif  // CHROME_BROWSER_UI_ASH_CAST_CONFIG_DELEGATE_MEDIA_ROUTER_H_
