// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_MEDIA_ROUTE_H_
#define CHROME_COMMON_MEDIA_ROUTER_MEDIA_ROUTE_H_

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"

namespace media_router {

// MediaRoute objects contain the status and metadata of a routing
// operation. The fields are immutable and reflect the route status
// only at the time of object creation. Updated route statuses must
// be retrieved as new MediaRoute objects from the Media Router.
//
// TODO(mfoltz): Convert to a simple struct and remove uncommon parameters from
// the ctor.
class MediaRoute {
 public:
  using Id = std::string;

  // |media_route_id|: ID of the route.
  // |media_source|: Description of source of the route.
  // |media_sink|: The sink that is receiving the media.
  // |description|: Description of the route to be displayed.
  // |is_local|: true if the route was created from this browser.
  // |custom_controller_path|: custom controller path if it is given by route
  //     provider. empty otherwise.
  // |for_display|: Set to true if this route should be displayed for
  //     |media_sink_id| in UI.
  MediaRoute(const MediaRoute::Id& media_route_id,
             const MediaSource& media_source,
             const MediaSink::Id& media_sink_id,
             const std::string& description,
             bool is_local,
             const std::string& custom_controller_path,
             bool for_display);
  MediaRoute(const MediaRoute& other);
  MediaRoute();

  ~MediaRoute();

  void set_media_route_id(const MediaRoute::Id& media_route_id) {
    media_route_id_ = media_route_id;
  }
  const MediaRoute::Id& media_route_id() const { return media_route_id_; }

  void set_media_source(const MediaSource& media_source) {
    media_source_ = media_source;
  }
  const MediaSource& media_source() const { return media_source_; }

  void set_media_sink_id(const MediaSink::Id& media_sink_id) {
    media_sink_id_ = media_sink_id;
  }
  const MediaSink::Id& media_sink_id() const { return media_sink_id_; }

  void set_description(const std::string& description) {
    description_ = description;
  }

  // TODO(kmarshall): Do we need to pass locale for bidi rendering?
  const std::string& description() const { return description_; }

  void set_local(bool is_local) { is_local_ = is_local; }
  bool is_local() const { return is_local_; }

  void set_custom_controller_path(const std::string& custom_controller_path) {
    custom_controller_path_ = custom_controller_path;
  }
  const std::string& custom_controller_path() const {
    return custom_controller_path_;
  }

  void set_supports_media_route_controller(
      bool supports_media_route_controller) {
    supports_media_route_controller_ = supports_media_route_controller;
  }
  bool supports_media_route_controller() const {
    return supports_media_route_controller_;
  }

  void set_for_display(bool for_display) { for_display_ = for_display; }
  bool for_display() const { return for_display_; }

  void set_incognito(bool is_incognito) { is_incognito_ = is_incognito; }
  bool is_incognito() const { return is_incognito_; }

  void set_offscreen_presentation(bool is_offscreen_presentation) {
    is_offscreen_presentation_ = is_offscreen_presentation;
  }
  bool is_offscreen_presentation() const { return is_offscreen_presentation_; }

  bool Equals(const MediaRoute& other) const;

 private:
  // The media route identifier.
  MediaRoute::Id media_route_id_;

  // The media source being routed.
  MediaSource media_source_;

  // The ID of sink being routed to.
  MediaSink::Id media_sink_id_;

  // The description of the media route activity, for example
  // "Playing Foo Bar Music All Access."
  std::string description_;

  // |true| if the route is created locally (versus discovered by a media route
  // provider.)
  bool is_local_ = false;

  // The custom controller path. This allows route provider to have custom route
  // detail as well as its own route control features route control features in
  // the media router dialog.
  std::string custom_controller_path_;

  // Whether the provider for this route supports the Media Route Controller.
  bool supports_media_route_controller_ = false;

  // |true| if the route can be displayed in the UI.
  bool for_display_ = false;

  // |true| if the route was created by an incognito profile.
  bool is_incognito_ = false;

  // |true| if the presentation associated with this route is an offscreen
  // presentation.
  bool is_offscreen_presentation_ = false;
};

}  // namespace media_router

#endif  // CHROME_COMMON_MEDIA_ROUTER_MEDIA_ROUTE_H_
