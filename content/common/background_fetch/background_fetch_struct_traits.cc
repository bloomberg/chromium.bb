// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/background_fetch/background_fetch_struct_traits.h"

#include "mojo/public/cpp/bindings/array_data_view.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::BackgroundFetchOptionsDataView,
                  content::BackgroundFetchOptions>::
    Read(blink::mojom::BackgroundFetchOptionsDataView data,
         content::BackgroundFetchOptions* options) {
  if (!data.ReadIcons(&options->icons) || !data.ReadTitle(&options->title))
    return false;

  options->total_download_size = data.total_download_size();
  return true;
}

// static
bool StructTraits<blink::mojom::BackgroundFetchRegistrationDataView,
                  content::BackgroundFetchRegistration>::
    Read(blink::mojom::BackgroundFetchRegistrationDataView data,
         content::BackgroundFetchRegistration* registration) {
  if (!data.ReadTag(&registration->tag) ||
      !data.ReadIcons(&registration->icons) ||
      !data.ReadTitle(&registration->title)) {
    return false;
  }

  registration->total_download_size = data.total_download_size();
  return true;
}

// static
bool StructTraits<
    blink::mojom::IconDefinitionDataView,
    content::IconDefinition>::Read(blink::mojom::IconDefinitionDataView data,
                                   content::IconDefinition* definition) {
  return data.ReadSrc(&definition->src) && data.ReadSizes(&definition->sizes) &&
         data.ReadType(&definition->type);
}

}  // namespace mojo
