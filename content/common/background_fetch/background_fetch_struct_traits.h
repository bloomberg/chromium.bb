// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_STRUCT_TRAITS_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_STRUCT_TRAITS_H_

#include <string>
#include <vector>

#include "content/common/background_fetch/background_fetch_types.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace mojo {

template <>
struct CONTENT_EXPORT
    StructTraits<blink::mojom::BackgroundFetchSettledFetchDataView,
                 content::BackgroundFetchSettledFetch> {
  static blink::mojom::FetchAPIRequestPtr request(
      const content::BackgroundFetchSettledFetch& fetch) {
    return content::BackgroundFetchSettledFetch::CloneRequest(fetch.request);
  }
  static blink::mojom::FetchAPIResponsePtr response(
      const content::BackgroundFetchSettledFetch& fetch) {
    return content::BackgroundFetchSettledFetch::CloneResponse(fetch.response);
  }

  static bool Read(blink::mojom::BackgroundFetchSettledFetchDataView data,
                   content::BackgroundFetchSettledFetch* definition);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_STRUCT_TRAITS_H_
