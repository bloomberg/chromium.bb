// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HISTOGRAM_INTERNALS_URL_LOADER_H_
#define CONTENT_BROWSER_HISTOGRAM_INTERNALS_URL_LOADER_H_

#include "content/common/url_loader.mojom.h"

namespace content {

// Creates the data for chrome://histograms.
void StartHistogramInternalsURLLoader(const ResourceRequest& request,
                                      mojom::URLLoaderClientPtr client);
}  // namespace content

#endif  // CONTENT_BROWSER_HISTOGRAM_INTERNALS_URL_LOADER_H_
