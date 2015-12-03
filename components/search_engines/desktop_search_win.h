// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_DESKTOP_SEARCH_WIN_H_
#define COMPONENTS_SEARCH_ENGINES_DESKTOP_SEARCH_WIN_H_

#include "base/feature_list.h"
#include "base/strings/string16.h"

class GURL;
class PrefService;
class SearchTermsData;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace prefs {
// Name of the Windows desktop search redirection preference.
extern const char kWindowsDesktopSearchRedirectionPref[];
}

// Windows desktop search redirection feature. This is exposed in the header
// file so that it can be referenced from about_flags.cc.
extern const base::Feature kWindowsDesktopSearchRedirectionFeature;

// Registers the Windows desktop search redirection preference into |registry|.
void RegisterWindowsDesktopSearchRedirectionPref(
    user_prefs::PrefRegistrySyncable* registry);

// Indicates whether Windows desktop searches should be redirected to the
// default search engine. This is only true when both the preference and the
// feature are enabled. The preference value is read from |pref_service|.
bool ShouldRedirectWindowsDesktopSearchToDefaultSearchEngine(
    PrefService* pref_service);

// Detects whether a URL comes from a Windows Desktop search. If so, puts the
// search terms in |search_terms| and returns true.
bool DetectWindowsDesktopSearch(const GURL& url,
                                const SearchTermsData& search_terms_data,
                                base::string16* search_terms);

#endif  // COMPONENTS_SEARCH_ENGINES_DESKTOP_SEARCH_WIN_H_
