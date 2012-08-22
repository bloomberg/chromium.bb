// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator_win.h"

#include "base/logging.h"
#include "base/win/metro.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"

namespace chrome {

GURL GetURLToOpen(Profile* profile) {
  string16 params;
  base::win::MetroLaunchType launch_type =
      base::win::GetMetroLaunchParams(&params);

  if ((launch_type == base::win::METRO_PROTOCOL) ||
      (launch_type == base::win::METRO_LAUNCH)) {
    return GURL(params);
  } else if (launch_type == base::win::METRO_SEARCH) {
    const TemplateURL* default_provider =
        TemplateURLServiceFactory::GetForProfile(profile)->
        GetDefaultSearchProvider();
    if (default_provider) {
      const TemplateURLRef& search_url = default_provider->url_ref();
      DCHECK(search_url.SupportsReplacement());
      return GURL(search_url.ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(params)));
    }
  }
  return GURL();
}

}  // namespace chrome
