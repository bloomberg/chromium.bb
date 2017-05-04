// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_test_base.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"

InstantTestBase::InstantTestBase()
    : browser_(nullptr),
      https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS),
      init_suggestions_url_(false) {
  https_test_server_.ServeFilesFromSourceDirectory("chrome/test/data");
}

InstantTestBase::~InstantTestBase() {}

void InstantTestBase::SetupInstant(Browser* browser) {
  browser_ = browser;

  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser_->profile());
  search_test_utils::WaitForTemplateURLServiceToLoad(service);

  TemplateURLData data;
  // Necessary to use exact URL for both the main URL and the alternate URL for
  // search term extraction to work in InstantExtended.
  data.SetShortName(base::ASCIIToUTF16("name"));
  data.SetURL(instant_url_.spec() + "q={searchTerms}&is_search");
  data.instant_url = instant_url_.spec();
  data.new_tab_url = ntp_url_.spec();
  if (init_suggestions_url_)
    data.suggestions_url = instant_url_.spec() + "#q={searchTerms}";
  data.alternate_urls.push_back(instant_url_.spec() + "#q={searchTerms}");
  data.search_terms_replacement_key = "strk";

  TemplateURL* template_url = service->Add(base::MakeUnique<TemplateURL>(data));
  service->SetUserSelectedDefaultSearchProvider(template_url);
}

void InstantTestBase::Init(const GURL& instant_url,
                           const GURL& ntp_url,
                           bool init_suggestions_url) {
  instant_url_ = instant_url;
  ntp_url_ = ntp_url;
  init_suggestions_url_ = init_suggestions_url;
}
