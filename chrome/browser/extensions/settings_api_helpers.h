// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SETTINGS_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_SETTINGS_API_HELPERS_H_

#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"

namespace content {
class BrowserContext;
}

namespace extensions {

struct SettingsOverrides;

// Find which |extension| is overriding a particular |type| of setting. Returns
// the SettingsOverride object, or NULL if no |extension| is overriding that
// particular setting.
const extensions::SettingsOverrides* FindOverridingExtension(
    content::BrowserContext* browser_context,
    SettingsApiOverrideType type,
    const Extension** extension);

// Returns which extension is overriding the homepage in a given
// |browser_context|. |home_page_url|, if non-NULL, will contain the home_page
// value the extension has set.
const Extension* OverridesHomepage(content::BrowserContext* browser_context,
                                   GURL* home_page_url);

// Returns which extension is overriding the homepage in a given
// |browser_context|. |startup_pages|, if non-NULL, will contain the vector of
// startup page URLs the extension has set.
const Extension* OverridesStartupPages(content::BrowserContext* browser_context,
                                       std::vector<GURL>* startup_pages);

// Returns which extension is overriding the search engine in a given
// |browser_context|. |search_provider|, if non-NULL, will contain the search
// provider the extension has set.
const Extension* OverridesSearchEngine(
    content::BrowserContext* browser_context,
    api::manifest_types::ChromeSettingsOverrides::Search_provider*
        search_provider);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SETTINGS_API_HELPERS_H_
