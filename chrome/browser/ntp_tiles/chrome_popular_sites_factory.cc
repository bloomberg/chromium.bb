// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_tiles/chrome_popular_sites_factory.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/ntp_tiles/popular_sites_impl.h"
#include "components/safe_json/safe_json_parser.h"
#include "content/public/browser/browser_thread.h"

std::unique_ptr<ntp_tiles::PopularSites>
ChromePopularSitesFactory::NewForProfile(Profile* profile) {
  return base::MakeUnique<ntp_tiles::PopularSitesImpl>(
      profile->GetPrefs(), TemplateURLServiceFactory::GetForProfile(profile),
      g_browser_process->variations_service(), profile->GetRequestContext(),
      base::Bind(safe_json::SafeJsonParser::Parse));
}
