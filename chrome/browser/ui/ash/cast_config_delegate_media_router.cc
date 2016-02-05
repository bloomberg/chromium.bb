// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/cast_config_delegate_media_router.h"

#include "base/macros.h"
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
std::string StripEndingTab(const std::string& str) {
  static const char ending[] = " (Tab)";
  if (base::EndsWith(str, ending, base::CompareCase::SENSITIVE))
    return str.substr(0, str.size() - strlen(ending));
  return str;
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
  using MediaRouteIds = std::vector<media_router::MediaRoute::Id>;

  explicit CastDeviceCache(ash::CastConfigDelegate* cast_config_delegate);
  ~CastDeviceCache() override;

  // This may call cast_config_delegate->RequestDeviceRefresh() before
  // returning.
  void Init();

  const MediaSinks& sinks() const { return sinks_; }
  const MediaRoutes& routes() const { return routes_; }

 private:
  // media_router::MediaSinksObserver:
  void OnSinksReceived(const MediaSinks& sinks) override;

  // media_router::MediaRoutesObserver:
  void OnRoutesUpdated(const MediaRoutes& routes,
                       const MediaRouteIds& unused_joinable_route_ids) override;

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
}

CastDeviceCache::~CastDeviceCache() {}

void CastDeviceCache::Init() {
  CHECK(MediaSinksObserver::Init());
}

void CastDeviceCache::OnSinksReceived(const MediaSinks& sinks) {
  sinks_ = sinks;
  cast_config_delegate_->RequestDeviceRefresh();
}

void CastDeviceCache::OnRoutesUpdated(
    const MediaRoutes& routes,
    const MediaRouteIds& unused_joinable_route_ids) {
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
  if (!devices_ && GetMediaRouter() != nullptr) {
    devices_.reset(new CastDeviceCache(this));
    devices_->Init();
  }

  return devices_.get();
}

bool CastConfigDelegateMediaRouter::HasCastExtension() const {
  return true;
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
        item.activity.title =
            base::UTF8ToUTF16(StripEndingTab(route.description()));
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

  FOR_EACH_OBSERVER(ash::CastConfigDelegate::Observer, observer_list_,
                    OnDevicesUpdated(items));
}

void CastConfigDelegateMediaRouter::CastToReceiver(
    const std::string& receiver_id) {
  // TODO(imcheng): Pass in tab casting timeout.
  GetMediaRouter()->CreateRoute(
      media_router::MediaSourceForDesktop().id(), receiver_id,
      GURL("http://cros-cast-origin/"), nullptr,
      std::vector<media_router::MediaRouteResponseCallback>(),
      base::TimeDelta());
}

void CastConfigDelegateMediaRouter::StopCasting(const std::string& route_id) {
  GetMediaRouter()->TerminateRoute(route_id);
}

bool CastConfigDelegateMediaRouter::HasOptions() const {
  // There are no plans to have an options page for the MediaRouter.
  return false;
}

void CastConfigDelegateMediaRouter::LaunchCastOptions() {}

void CastConfigDelegateMediaRouter::AddObserver(
    ash::CastConfigDelegate::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void CastConfigDelegateMediaRouter::RemoveObserver(
    ash::CastConfigDelegate::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}
