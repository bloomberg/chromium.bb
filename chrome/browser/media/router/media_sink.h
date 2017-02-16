// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_H_

#include <string>

#include "base/optional.h"
#include "third_party/icu/source/common/unicode/uversion.h"

namespace U_ICU_NAMESPACE {
class Collator;
}  // namespace U_ICU_NAMESPACE

namespace media_router {

// Represents a sink to which media can be routed.
class MediaSink {
 public:
  using Id = std::string;

  // IconTypes are listed in the order in which sinks should be sorted.
  // The order must stay in sync with
  // chrome/browser/resources/media_router/media_router_data.js.
  enum IconType {
    CAST,
    CAST_AUDIO_GROUP,
    CAST_AUDIO,
    HANGOUT,
    GENERIC
  };

  MediaSink(const MediaSink::Id& sink_id,
            const std::string& name,
            const IconType icon_type);
  MediaSink(const MediaSink& other);
  MediaSink();

  ~MediaSink();

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

  void set_icon_type(IconType icon_type) { icon_type_ = icon_type; }
  IconType icon_type() const { return icon_type_; }

  bool Equals(const MediaSink& other) const;

  // Compares |this| to |other| first by their icon types, then their names
  // using |collator|, and finally their IDs.
  bool CompareUsingCollator(const MediaSink& other,
                            const icu::Collator* collator) const;

  // For storing in sets and in maps as keys.
  struct Compare {
    bool operator()(const MediaSink& sink1, const MediaSink& sink2) const {
      return sink1.id() < sink2.id();
    }
  };

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
  IconType icon_type_ = IconType::GENERIC;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_SINK_H_
