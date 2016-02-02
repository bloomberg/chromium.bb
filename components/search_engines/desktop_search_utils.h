// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_DESKTOP_SEARCH_UTILS_H_
#define COMPONENTS_SEARCH_ENGINES_DESKTOP_SEARCH_UTILS_H_

#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

class PrefService;
class TemplateURLService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace prefs {
// Name of the preference keeping track of whether the desktop search
// redirection infobar has been shown.
extern const char kDesktopSearchRedirectionInfobarShownPref[];
}

// Desktop search redirection feature. This is exposed in the header file so
// that it can be referenced from about_flags.cc.
extern const base::Feature kDesktopSearchRedirectionFeature;

// Registers the preference keeping track of whether the desktop search
// redirection infobar has been shown.
void RegisterDesktopSearchRedirectionPref(
    user_prefs::PrefRegistrySyncable* registry);

// Replaces |url| by a default search engine URL if:
// - |url| is a desktop search URL.
// - The desktop search redirection feature is enabled.
// - The default search engine is not Bing.
// Returns true if an infobar should be shown to tell the user about the
// redirection.
bool ReplaceDesktopSearchURLWithDefaultSearchURLIfNeeded(
    const PrefService* pref_service,
    TemplateURLService* template_url_service,
    GURL* url);

#endif  // COMPONENTS_SEARCH_ENGINES_DESKTOP_SEARCH_UTILS_H_
