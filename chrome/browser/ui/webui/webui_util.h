// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "chrome/common/buildflags.h"

namespace content {
class WebUIDataSource;
}

struct GritResourceMap;

namespace webui {

// Performs common setup steps for |source|, assuming it is using Polymer 3,
// by adding all resources, setting the default resource, setting up i18n,
// and ensuring that tests work correctly by updating the CSP and adding the
// test loader files.
void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const GritResourceMap> resources,
                          const std::string& generated_path,
                          int default_resource);

#if BUILDFLAG(OPTIMIZE_WEBUI)
// Same as SetupWebUIDataSource, but for a bundled page; this adds only the
// bundle and the default resource to |source|.
void SetupBundledWebUIDataSource(content::WebUIDataSource* source,
                                 base::StringPiece bundled_path,
                                 int bundle,
                                 int default_resource);
#endif

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
