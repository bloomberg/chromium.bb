// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_switches.h"

namespace switches {

// Additional query params to insert in the search and instant URLs.  Useful for
// testing.
const char kExtraSearchQueryParams[] = "extra-search-query-params";

#if defined(OS_WIN)
// Use the default search provider for desktop search.
const char kUseDefaultSearchProviderForDesktopSearch[] =
    "use-default-search-provider-for-desktop-search";
#endif  // defined(OS_WIN)

}  // namespace switches
