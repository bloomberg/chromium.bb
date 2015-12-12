// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/cast_config_delegate_media_router.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/media_routes_observer.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace {

media_router::MediaRouter* media_router_for_test_ = nullptr;

media_router::MediaRouter* GetMediaRouter() {
  if (media_router_for_test_)
    return media_router_for_test_;

  auto* router = media_router::MediaRouterFactory::GetApiForBrowserContext(
      ProfileManager::GetPrimaryUserProfile());
  DCHECK(router);
  return router;
}

// The media router will sometimes append " (Tab)" to the tab title. This
// function will remove that data from the inout param |string|.
void StripEndingTab(base::string16* string) {
  const base::string16 ending = base::UTF8ToUTF16(" (Tab)");
  if (base::EndsWith(*string, ending, base::CompareCase::SENSITIVE))
    *string = string->substr(0, string->size() - ending.size());
}

}  // namespace

// This class caches the values that the observers give us so we can query them
// at any point in time. It also emits a device refresh event when new data is
// available.
class CastDeviceCache : public media_router::MediaRoutesObserver,
                        public media_router::MediaSinksObserver {
 public:
  using MediaSinks = std::vector<media_router::MediaSink>;
  using MediaRoutes = std::vector<media_router::MediaRoute>;

  explicit CastDeviceCache(ash::CastConfigDelegate* cast_config_delegate);
  ~CastDeviceCache() override;

  const MediaSinks& sinks() const { return sinks_; }
  const MediaRoutes& routes() const { return routes_; }

 private:
  // media_router::MediaSinksObserver:
  void OnSinksReceived(const MediaSinks& sinks) override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(const MediaRoutes& routes) override;

  MediaSinks sinks_;
  MediaRoutes routes_;

  // Not owned.
  ash::CastConfigDelegate* cast_config_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CastDeviceCache);
};

CastDeviceCache::CastDeviceCache(ash::CastConfigDelegate* cast_config_delegate)
    : MediaRoutesObserver(GetMediaRouter()),
      MediaSinksObserver(GetMediaRouter(),
                         media_router::MediaSourceForDesktop()),
      cast_config_delegate_(cast_config_delegate) {
  CHECK(MediaSinksObserver::Init());
}

CastDeviceCache::~CastDeviceCache() {}

void CastDeviceCache::OnSinksReceived(const MediaSinks& sinks) {
  sinks_ = sinks;
  cast_config_delegate_->RequestDeviceRefresh();
}

void CastDeviceCache::OnRoutesUpdated(const MediaRoutes& routes) {
  routes_ = routes;
  cast_config_delegate_->RequestDeviceRefresh();
}

////////////////////////////////////////////////////////////////////////////////
// CastConfigDelegateMediaRouter:

// static
bool CastConfigDelegateMediaRouter::IsEnabled() {
  return media_router::MediaRouterEnabled(
             ProfileManager::GetPrimaryUserProfile()) ||
         media_router_for_test_;
}

void CastConfigDelegateMediaRouter::SetMediaRouterForTest(
    media_router::MediaRouter* media_router) {
  media_router_for_test_ = media_router;
}

CastConfigDelegateMediaRouter::CastConfigDelegateMediaRouter() {}

CastConfigDelegateMediaRouter::~CastConfigDelegateMediaRouter() {}

CastDeviceCache* CastConfigDelegateMediaRouter::devices() {
  // The CastDeviceCache instance is lazily allocated because the MediaRouter
  // component is not ready when the constructor is invoked.
  if (!devices_ && GetMediaRouter() != nullptr)
    devices_.reset(new CastDeviceCache(this));

  return devices_.get();
}

bool CastConfigDelegateMediaRouter::HasCastExtension() const {
  return true;
}

CastConfigDelegateMediaRouter::DeviceUpdateSubscription
CastConfigDelegateMediaRouter::RegisterDeviceUpdateObserver(
    const ReceiversAndActivitesCallback& callback) {
  return callback_list_.Add(callback);
}

void CastConfigDelegateMediaRouter::RequestDeviceRefresh() {
  // The media router component isn't ready yet.
  if (!devices())
    return;

  // Build the old-style ReceiverAndActivity set out of the MediaRouter
  // source/sink/route setup. We first map the existing sinks, and then we
  // update those sinks with activity information.

  ReceiversAndActivities items;

  for (const media_router::MediaSink& sink : devices()->sinks()) {
    ReceiverAndActivity ra;
    ra.receiver.id = sink.id();
    ra.receiver.name = base::UTF8ToUTF16(sink.name());
    items.push_back(ra);
  }

  for (const media_router::MediaRoute& route : devices()->routes()) {
    if (!route.for_display())
      continue;

    for (ReceiverAndActivity& item : items) {
      if (item.receiver.id == route.media_sink_id()) {
        item.activity.id = route.media_route_id();
        item.activity.title = base::UTF8ToUTF16(route.description());
        StripEndingTab(&item.activity.title);
        item.activity.is_local_source = route.is_local();

        if (route.is_local()) {
          // TODO(jdufault): Once the extension backend is removed, we can
          // remove tab_id and specify the Desktop/Tab capture directly.
          // crbug.com/551132.
          // TODO(jdufault): We currently don't actually display DIAL casts to
          // the user even though we have all the information necessary. We'll
          // do this once the extension backend is gone because supporting both
          // introduces extra complexity. crbug.com/551132.

          // Default to a tab/app capture. This will display the media router
          // description. This means we will properly support DIAL casts.
          item.activity.tab_id = 0;
          if (media_router::IsDesktopMirroringMediaSource(route.media_source()))
            item.activity.tab_id = Activity::TabId::DESKTOP;
        }

        break;
      }
    }
  }

  callback_list_.Notify(items);
}

void CastConfigDelegateMediaRouter::CastToReceiver(
    const std::string& receiver_id) {
  GetMediaRouter()->CreateRoute(
      media_router::MediaSourceForDesktop().id(), receiver_id,
      GURL("http://cros-cast-origin/"), nullptr,
      std::vector<media_router::MediaRouteResponseCallback>());
}

void CastConfigDelegateMediaRouter::StopCasting(const std::string& route_id) {
  GetMediaRouter()->TerminateRoute(route_id);
}

bool CastConfigDelegateMediaRouter::HasOptions() const {
  // There are no plans to have an options page for the MediaRouter.
  return false;
}

void CastConfigDelegateMediaRouter::LaunchCastOptions() {}
