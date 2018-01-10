// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/wired_display/wired_display_media_route_provider.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/number_formatting.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_presentation_receiver.h"
#include "chrome/browser/media/router/providers/wired_display/wired_display_presentation_receiver_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/media_router/media_source_helper.h"
#include "chrome/common/media_router/route_request_result.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

using display::Display;

namespace media_router {

namespace {

std::string GetSinkIdForDisplay(const Display& display) {
  return WiredDisplayMediaRouteProvider::kSinkPrefix +
         std::to_string(display.id());
}

bool IsPresentationSource(const std::string& media_source) {
  const GURL source_url(media_source);
  return source_url.is_valid() && source_url.SchemeIsHTTPOrHTTPS() &&
         !base::StartsWith(source_url.spec(), kLegacyCastPresentationUrlPrefix,
                           base::CompareCase::INSENSITIVE_ASCII);
}

MediaSinkInternal CreateSinkForDisplay(const Display& display,
                                       int display_index) {
  const std::string sink_id = GetSinkIdForDisplay(display);
  const std::string sink_name =
      l10n_util::GetStringFUTF8(IDS_MEDIA_ROUTER_WIRED_DISPLAY_SINK_NAME,
                                base::FormatNumber(display_index));
  MediaSink sink(sink_id, sink_name, SinkIconType::GENERIC,
                 MediaRouteProviderId::WIRED_DISPLAY);
  MediaSinkInternal sink_internal;
  sink_internal.set_sink(sink);
  return sink_internal;
}

// Returns true if |display1| should come before |display2| when displays are
// sorted. Primary displays and displays to the top-left take priority, in
// that order.
bool CompareDisplays(int64_t primary_id,
                     const Display& display1,
                     const Display& display2) {
  if (display1.id() == primary_id)
    return true;
  if (display2.id() == primary_id)
    return false;
  return display1.bounds().y() < display2.bounds().y() ||
         (display1.bounds().y() == display2.bounds().y() &&
          display1.bounds().x() < display2.bounds().x());
}

}  // namespace

// static
const MediaRouteProviderId WiredDisplayMediaRouteProvider::kProviderId =
    MediaRouteProviderId::WIRED_DISPLAY;

// static
const char WiredDisplayMediaRouteProvider::kSinkPrefix[] = "wired_display_";

WiredDisplayMediaRouteProvider::WiredDisplayMediaRouteProvider(
    mojom::MediaRouteProviderRequest request,
    mojom::MediaRouterPtr media_router,
    Profile* profile)
    : binding_(this, std::move(request)),
      media_router_(std::move(media_router)),
      profile_(profile) {
  display::Screen::GetScreen()->AddObserver(this);
  ReportSinkAvailability(GetSinks());
}

WiredDisplayMediaRouteProvider::~WiredDisplayMediaRouteProvider() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

void WiredDisplayMediaRouteProvider::CreateRoute(
    const std::string& media_source,
    const std::string& sink_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    CreateRouteCallback callback) {
#if defined(OS_MACOSX)
  // TODO(https://crbug.com/777654): Support presenting to macOS as well.
  std::move(callback).Run(base::nullopt, std::string("Not implemented"),
                          RouteRequestResult::UNKNOWN_ERROR);
  return;
#endif
  DCHECK(!base::ContainsKey(presentations_, presentation_id));
  base::Optional<Display> display = GetDisplayBySinkId(sink_id);
  if (!display) {
    std::move(callback).Run(base::nullopt, std::string("Display not found"),
                            RouteRequestResult::SINK_NOT_FOUND);
    return;
  }

  // If there already is a presentation on |display|, terminate it.
  TerminatePresentationsOnDisplay(*display);
  // Use |presentation_id| as the route ID. This MRP creates only one route per
  // presentation ID.
  MediaRoute route(presentation_id, MediaSource(media_source), sink_id, "",
                   true, true);
  route.set_local_presentation(true);
  route.set_incognito(profile_->IsOffTheRecord());
  Presentation presentation =
      CreatePresentation(presentation_id, *display, route);

  presentation.receiver->Start(presentation_id, GURL(media_source));
  presentations_.emplace(presentation_id, std::move(presentation));
  std::move(callback).Run(route, base::nullopt, RouteRequestResult::OK);
  NotifyRouteObservers();
}

void WiredDisplayMediaRouteProvider::JoinRoute(
    const std::string& media_source,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    JoinRouteCallback callback) {
  std::move(callback).Run(
      base::nullopt,
      std::string("Join should be handled by the presentation manager"),
      RouteRequestResult::UNKNOWN_ERROR);
}

void WiredDisplayMediaRouteProvider::ConnectRouteByRouteId(
    const std::string& media_source,
    const std::string& route_id,
    const std::string& presentation_id,
    const url::Origin& origin,
    int32_t tab_id,
    base::TimeDelta timeout,
    bool incognito,
    ConnectRouteByRouteIdCallback callback) {
  std::move(callback).Run(
      base::nullopt,
      std::string("Connect should be handled by the presentation manager"),
      RouteRequestResult::UNKNOWN_ERROR);
}

void WiredDisplayMediaRouteProvider::TerminateRoute(
    const std::string& route_id,
    TerminateRouteCallback callback) {
  auto it = presentations_.find(route_id);
  if (it == presentations_.end()) {
    std::move(callback).Run(std::string("Presentation not found"),
                            RouteRequestResult::ROUTE_NOT_FOUND);
    return;
  }

  // The presentation will be removed from |presentations_| in the termination
  // callback of its receiver.
  it->second.receiver->Terminate();
  std::move(callback).Run(base::nullopt, RouteRequestResult::OK);
}

void WiredDisplayMediaRouteProvider::SendRouteMessage(
    const std::string& media_route_id,
    const std::string& message,
    SendRouteMessageCallback callback) {
  // Messages should be handled by LocalPresentationManager.
  NOTREACHED();
  std::move(callback).Run(false);
}

void WiredDisplayMediaRouteProvider::SendRouteBinaryMessage(
    const std::string& media_route_id,
    const std::vector<uint8_t>& data,
    SendRouteBinaryMessageCallback callback) {
  // Messages should be handled by LocalPresentationManager.
  NOTREACHED();
  std::move(callback).Run(false);
}

void WiredDisplayMediaRouteProvider::StartObservingMediaSinks(
    const std::string& media_source) {
  if (IsPresentationSource(media_source))
    sink_queries_.insert(media_source);
  UpdateMediaSinks(media_source);
}

void WiredDisplayMediaRouteProvider::StopObservingMediaSinks(
    const std::string& media_source) {
  sink_queries_.erase(media_source);
}

void WiredDisplayMediaRouteProvider::StartObservingMediaRoutes(
    const std::string& media_source) {
  route_queries_.insert(media_source);
  std::vector<MediaRoute> route_list;
  for (const auto& presentation : presentations_)
    route_list.push_back(presentation.second.route);
  media_router_->OnRoutesUpdated(kProviderId, route_list, media_source, {});
}

void WiredDisplayMediaRouteProvider::StopObservingMediaRoutes(
    const std::string& media_source) {
  route_queries_.erase(media_source);
}

void WiredDisplayMediaRouteProvider::StartListeningForRouteMessages(
    const std::string& route_id) {
  // Messages should be handled by LocalPresentationManager.
}

void WiredDisplayMediaRouteProvider::StopListeningForRouteMessages(
    const std::string& route_id) {
  // Messages should be handled by LocalPresentationManager.
}

void WiredDisplayMediaRouteProvider::DetachRoute(const std::string& route_id) {
  // Detaching should be handled by LocalPresentationManager.
  NOTREACHED();
}

void WiredDisplayMediaRouteProvider::EnableMdnsDiscovery() {}

void WiredDisplayMediaRouteProvider::UpdateMediaSinks(
    const std::string& media_source) {
  if (IsPresentationSource(media_source))
    media_router_->OnSinksReceived(kProviderId, media_source, GetSinks(), {});
}

void WiredDisplayMediaRouteProvider::SearchSinks(
    const std::string& sink_id,
    const std::string& media_source,
    mojom::SinkSearchCriteriaPtr search_criteria,
    SearchSinksCallback callback) {
  // The use of this method is not required by this MRP.
  std::move(callback).Run("");
}

void WiredDisplayMediaRouteProvider::ProvideSinks(
    const std::string& provider_name,
    const std::vector<media_router::MediaSinkInternal>& sinks) {
  NOTREACHED();
}

void WiredDisplayMediaRouteProvider::CreateMediaRouteController(
    const std::string& route_id,
    mojom::MediaControllerRequest media_controller,
    mojom::MediaStatusObserverPtr observer,
    CreateMediaRouteControllerCallback callback) {
  // Local screens do not support media controls.
  std::move(callback).Run(false);
}

void WiredDisplayMediaRouteProvider::OnDidProcessDisplayChanges() {
  NotifySinkObservers();
}

void WiredDisplayMediaRouteProvider::OnDisplayAdded(
    const Display& new_display) {
  NotifySinkObservers();
}

void WiredDisplayMediaRouteProvider::OnDisplayRemoved(
    const Display& old_display) {
  NotifySinkObservers();
}

void WiredDisplayMediaRouteProvider::OnDisplayMetricsChanged(
    const Display& display,
    uint32_t changed_metrics) {
  NotifySinkObservers();
}

std::vector<Display> WiredDisplayMediaRouteProvider::GetAllDisplays() const {
  return display::Screen::GetScreen()->GetAllDisplays();
}

Display WiredDisplayMediaRouteProvider::GetPrimaryDisplay() const {
  return display::Screen::GetScreen()->GetPrimaryDisplay();
}

WiredDisplayMediaRouteProvider::Presentation::Presentation(
    const MediaRoute& route,
    std::unique_ptr<WiredDisplayPresentationReceiver> receiver)
    : route(route), receiver(std::move(receiver)) {}

WiredDisplayMediaRouteProvider::Presentation::Presentation(
    Presentation&& other) = default;

WiredDisplayMediaRouteProvider::Presentation::~Presentation() = default;

void WiredDisplayMediaRouteProvider::NotifyRouteObservers() const {
  std::vector<MediaRoute> route_list;
  for (const auto& presentation : presentations_)
    route_list.push_back(presentation.second.route);
  for (const auto& route_query : route_queries_)
    media_router_->OnRoutesUpdated(kProviderId, route_list, route_query, {});
}

void WiredDisplayMediaRouteProvider::NotifySinkObservers() {
  std::vector<MediaSinkInternal> sinks = GetSinks();
  device_count_metrics_.RecordDeviceCountsIfNeeded(sinks.size(), sinks.size());
  ReportSinkAvailability(sinks);
  for (const auto& sink_query : sink_queries_)
    media_router_->OnSinksReceived(kProviderId, sink_query, sinks, {});
}

std::vector<MediaSinkInternal> WiredDisplayMediaRouteProvider::GetSinks()
    const {
  std::vector<MediaSinkInternal> sinks;
  std::vector<Display> displays = GetAvailableDisplays();
  for (size_t i = 0; i < displays.size(); i++)
    sinks.push_back(CreateSinkForDisplay(displays[i], i + 1));
  return sinks;
}

std::vector<Display> WiredDisplayMediaRouteProvider::GetAvailableDisplays()
    const {
  std::vector<Display> displays = GetAllDisplays();
  const Display primary_display = GetPrimaryDisplay();
  std::sort(
      displays.begin(), displays.end(),
      [&primary_display](const Display& display1, const Display& display2) {
        return CompareDisplays(primary_display.id(), display1, display2);
      });

  // Remove displays that mirror the primary display. On some platforms such as
  // Windows, mirrored displays are reported as one display. On others, mirrored
  // displays are reported separately but with the same bounds.
  base::EraseIf(displays, [&primary_display](const Display& display) {
    return display.id() != primary_display.id() &&
           display.bounds() == primary_display.bounds();
  });
  // If there is only one display, the user should not be able to present to it.
  return displays.size() == 1 ? std::vector<Display>() : displays;
}

void WiredDisplayMediaRouteProvider::ReportSinkAvailability(
    const std::vector<MediaSinkInternal>& sinks) {
  mojom::MediaRouter::SinkAvailability sink_availability =
      sinks.empty() ? mojom::MediaRouter::SinkAvailability::UNAVAILABLE
                    : mojom::MediaRouter::SinkAvailability::PER_SOURCE;
  media_router_->OnSinkAvailabilityUpdated(kProviderId, sink_availability);
}

void WiredDisplayMediaRouteProvider::RemovePresentationById(
    const std::string& presentation_id) {
  presentations_.erase(presentation_id);
  NotifyRouteObservers();
}

void WiredDisplayMediaRouteProvider::UpdateRouteDescription(
    const std::string& presentation_id,
    const std::string& title) {
  auto it = presentations_.find(presentation_id);
  if (it == presentations_.end())
    return;

  MediaRoute& route = it->second.route;
  if (title == route.description())
    return;

  route.set_description(title);
  NotifyRouteObservers();
}

WiredDisplayMediaRouteProvider::Presentation
WiredDisplayMediaRouteProvider::CreatePresentation(
    const std::string& presentation_id,
    const Display& display,
    const MediaRoute& route) {
  std::unique_ptr<WiredDisplayPresentationReceiver> receiver =
      WiredDisplayPresentationReceiverFactory::Create(
          profile_, display.bounds(),
          base::BindOnce(
              &WiredDisplayMediaRouteProvider::RemovePresentationById,
              base::Unretained(this), presentation_id),
          base::BindRepeating(
              &WiredDisplayMediaRouteProvider::UpdateRouteDescription,
              base::Unretained(this), presentation_id));
  return Presentation(route, std::move(receiver));
}

void WiredDisplayMediaRouteProvider::TerminatePresentationsOnDisplay(
    const display::Display& display) {
  std::vector<WiredDisplayPresentationReceiver*> presentations_to_terminate;
  // We cannot call Terminate() on the receiver while iterating over
  // |presentations_| because that might invoke a callback to delete the
  // presentation from |presentations_|.
  for (const auto& presentation : presentations_) {
    if (presentation.second.route.media_sink_id() ==
        GetSinkIdForDisplay(display)) {
      presentations_to_terminate.push_back(presentation.second.receiver.get());
    }
  }
  for (auto* presentation_to_terminate : presentations_to_terminate)
    presentation_to_terminate->Terminate();
}

base::Optional<Display> WiredDisplayMediaRouteProvider::GetDisplayBySinkId(
    const std::string& sink_id) const {
  std::vector<Display> displays = GetAllDisplays();
  auto it = std::find_if(displays.begin(), displays.end(),
                         [&sink_id](const Display& d) {
                           return GetSinkIdForDisplay(d) == sink_id;
                         });
  return it == displays.end() ? base::nullopt
                              : base::make_optional<Display>(std::move(*it));
}

}  // namespace media_router
