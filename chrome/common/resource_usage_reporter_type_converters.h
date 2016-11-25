// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RESOURCE_USAGE_REPORTER_TYPE_CONVERTERS_H_
#define CHROME_COMMON_RESOURCE_USAGE_REPORTER_TYPE_CONVERTERS_H_

#include "chrome/common/resource_usage_reporter.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/WebKit/public/web/WebCache.h"

namespace mojo {

template <>
struct TypeConverter<chrome::mojom::ResourceTypeStatsPtr,
                     blink::WebCache::ResourceTypeStats> {
  static chrome::mojom::ResourceTypeStatsPtr Convert(
      const blink::WebCache::ResourceTypeStats& obj);
};

template <>
struct TypeConverter<blink::WebCache::ResourceTypeStats,
                     chrome::mojom::ResourceTypeStats> {
  static blink::WebCache::ResourceTypeStats Convert(
      const chrome::mojom::ResourceTypeStats& obj);
};

}  // namespace mojo

#endif  // CHROME_COMMON_RESOURCE_USAGE_REPORTER_TYPE_CONVERTERS_H_
