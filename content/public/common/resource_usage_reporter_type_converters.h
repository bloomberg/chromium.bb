// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_USAGE_REPORTER_TYPE_CONVERTERS_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_USAGE_REPORTER_TYPE_CONVERTERS_H_

#include "content/common/content_export.h"
#include "content/public/common/resource_usage_reporter.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/WebKit/public/platform/WebCache.h"

namespace mojo {

template <>
struct CONTENT_EXPORT TypeConverter<content::mojom::ResourceTypeStatsPtr,
                                    blink::WebCache::ResourceTypeStats> {
  static content::mojom::ResourceTypeStatsPtr Convert(
      const blink::WebCache::ResourceTypeStats& obj);
};

template <>
struct CONTENT_EXPORT TypeConverter<blink::WebCache::ResourceTypeStats,
                                    content::mojom::ResourceTypeStats> {
  static blink::WebCache::ResourceTypeStats Convert(
      const content::mojom::ResourceTypeStats& obj);
};

}  // namespace mojo

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_USAGE_REPORTER_TYPE_CONVERTERS_H_
