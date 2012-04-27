// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_init_win.h"

#include "base/logging.h"
#include "base/win/metro.h"

#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"

namespace browser {

// Metro driver exports for getting the launch type, initial url, initial
// search term, etc.
extern "C" {
typedef const wchar_t* (*GetInitialUrl)();
typedef const wchar_t* (*GetInitialSearchString)();
typedef base::win::MetroLaunchType (*GetLaunchType)(
    base::win::MetroPreviousExecutionState* previous_state);
}

GURL GetURLToOpen(Profile* profile) {
  HMODULE metro = base::win::GetMetroModule();
  if (!metro)
    return GURL();

  GetLaunchType get_launch_type = reinterpret_cast<GetLaunchType>(
      ::GetProcAddress(metro, "GetLaunchType"));
  DCHECK(get_launch_type);

  base::win::MetroLaunchType launch_type = get_launch_type(NULL);

  if (launch_type == base::win::PROTOCOL) {
    GetInitialUrl initial_metro_url = reinterpret_cast<GetInitialUrl>(
        ::GetProcAddress(metro, "GetInitialUrl"));
    DCHECK(initial_metro_url);
    const wchar_t* initial_url = initial_metro_url();
    if (initial_url)
      return GURL(initial_url);
  } else if (launch_type == base::win::SEARCH) {
    GetInitialSearchString initial_search_string =
        reinterpret_cast<GetInitialSearchString>(
            ::GetProcAddress(metro, "GetInitialSearchString"));
    DCHECK(initial_search_string);
    string16 search_string = initial_search_string();

    const TemplateURL* default_provider =
        TemplateURLServiceFactory::GetForProfile(profile)->
        GetDefaultSearchProvider();
    if (default_provider) {
      const TemplateURLRef& search_url = default_provider->url_ref();
      DCHECK(search_url.SupportsReplacement());
      return GURL(search_url.ReplaceSearchTerms(search_string,
                      TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
    }
  }
  return GURL();
}

}  // namespace browser

