// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/resource_usage_reporter_type_converters.h"

#include "base/numerics/safe_conversions.h"

namespace mojo {

namespace {

ResourceTypeStatPtr StatToMojo(const blink::WebCache::ResourceTypeStat& obj) {
  ResourceTypeStatPtr stat = ResourceTypeStat::New();
  stat->count = obj.count;
  stat->size = obj.size;
  stat->live_size = obj.liveSize;
  stat->decoded_size = obj.decodedSize;
  return stat.Pass();
}

blink::WebCache::ResourceTypeStat StatFromMojo(const ResourceTypeStat& obj) {
  blink::WebCache::ResourceTypeStat stat;
  stat.count = base::saturated_cast<size_t>(obj.count);
  stat.size = base::saturated_cast<size_t>(obj.size);
  stat.liveSize = base::saturated_cast<size_t>(obj.live_size);
  stat.decodedSize = base::saturated_cast<size_t>(obj.decoded_size);
  return stat;
}

}  // namespace

// static
ResourceTypeStatsPtr
TypeConverter<ResourceTypeStatsPtr, blink::WebCache::ResourceTypeStats>::
    Convert(const blink::WebCache::ResourceTypeStats& obj) {
  ResourceTypeStatsPtr stats = ResourceTypeStats::New();
  stats->images = StatToMojo(obj.images);
  stats->css_style_sheets = StatToMojo(obj.cssStyleSheets);
  stats->scripts = StatToMojo(obj.scripts);
  stats->xsl_style_sheets = StatToMojo(obj.xslStyleSheets);
  stats->fonts = StatToMojo(obj.fonts);
  stats->other = StatToMojo(obj.other);
  return stats.Pass();
}

// static
blink::WebCache::ResourceTypeStats
TypeConverter<blink::WebCache::ResourceTypeStats, ResourceTypeStats>::Convert(
    const ResourceTypeStats& obj) {
  if (!obj.images || !obj.css_style_sheets || !obj.scripts ||
      !obj.xsl_style_sheets || !obj.fonts || !obj.other) {
    return {};
  }
  blink::WebCache::ResourceTypeStats stats;
  stats.images = StatFromMojo(*obj.images);
  stats.cssStyleSheets = StatFromMojo(*obj.css_style_sheets);
  stats.scripts = StatFromMojo(*obj.scripts);
  stats.xslStyleSheets = StatFromMojo(*obj.xsl_style_sheets);
  stats.fonts = StatFromMojo(*obj.fonts);
  stats.other = StatFromMojo(*obj.other);
  return stats;
}

}  // namespace mojo
