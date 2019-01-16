// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_ROUTER_MEDIA_SINK_H_
#define CHROME_COMMON_MEDIA_ROUTER_MEDIA_SINK_H_

#include <string>

#include "base/optional.h"
#include "chrome/common/media_router/media_route_provider_helper.h"
#include "third_party/icu/source/common/unicode/uversion.h"

namespace U_ICU_NAMESPACE {
class Collator;
}  // namespace U_ICU_NAMESPACE

namespace media_router {

// IconTypes are listed in the order in which sinks should be sorted.
// The order must stay in sync with
// chrome/browser/resources/media_router/media_router_data.js.
//
// NOTE: This enum is used for recording the MediaRouter.Sink.SelectedType
// metrics, so if we want to reorder it, we must create a separate enum that
// preserves the ordering, and map from this enum to the new one in
// MediaRouterMetrics::RecordMediaSinkType().
enum class SinkIconType {
  CAST = 0,
  CAST_AUDIO_GROUP = 1,
  CAST_AUDIO = 2,
  MEETING = 3,
  HANGOUT = 4,
  EDUCATION = 5,
  WIRED_DISPLAY = 6,
  GENERIC = 7,
  TOTAL_COUNT = 8  // Add new types above this line.
};

// Represents a sink to which media can be routed.
// TODO(zhaobin): convert MediaSink into a struct.
class MediaSink {
 public:
  using Id = std::string;

  // TODO(takumif): Remove the default argument for |provider_id|.
  MediaSink(const MediaSink::Id& sink_id,
            const std::string& name,
            SinkIconType icon_type,
            MediaRouteProviderId provider_id = MediaRouteProviderId::UNKNOWN);
  MediaSink(const MediaSink& other);
  MediaSink(MediaSink&& other) noexcept;
  MediaSink();
  ~MediaSink();

  MediaSink& operator=(const MediaSink& other);
  MediaSink& operator=(MediaSink&& other) noexcept;

  void set_sink_id(const MediaSink::Id& sink_id) { sink_id_ = sink_id; }
  const MediaSink::Id& id() const { return sink_id_; }

  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  void set_description(const std::string& description) {
    description_ = description;
  }
  const base::Optional<std::string>& description() const {
    return description_;
  }

  void set_domain(const std::string& domain) { domain_ = domain; }
  const base::Optional<std::string>& domain() const { return domain_; }

  void set_icon_type(SinkIconType icon_type) { icon_type_ = icon_type; }
  SinkIconType icon_type() const { return icon_type_; }

  void set_provider_id(MediaRouteProviderId provider_id) {
    provider_id_ = provider_id;
  }
  MediaRouteProviderId provider_id() const { return provider_id_; }

  // Returns true if the sink is from the Cloud MRP; however, as this is based
  // solely on the icon type, is not guaranteed to be correct 100% of the time.
  bool IsMaybeCloudSink() const;

  bool operator==(const MediaSink& other) const;
  bool operator!=(const MediaSink& other) const;

  // Compares |this| to |other| first by their icon types, then their names
  // using |collator|, and finally their IDs.
  bool CompareUsingCollator(const MediaSink& other,
                            const icu::Collator* collator) const;

 private:
  // Unique identifier for the MediaSink.
  MediaSink::Id sink_id_;

  // Descriptive name of the MediaSink.
  std::string name_;

  // Optional description of the MediaSink.
  base::Optional<std::string> description_;

  // Optional domain of the MediaSink.
  base::Optional<std::string> domain_;

  // The type of icon that corresponds with the MediaSink.
  SinkIconType icon_type_ = SinkIconType::GENERIC;

  // The ID of the MediaRouteProvider that the MediaSink belongs to.
  MediaRouteProviderId provider_id_ = MediaRouteProviderId::UNKNOWN;
};

}  // namespace media_router

#endif  // CHROME_COMMON_MEDIA_ROUTER_MEDIA_SINK_H_
