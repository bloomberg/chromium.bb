// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_data.h"

#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

TemplateURLData::TemplateURLData()
    : safe_for_autoreplace(false),
      id(0),
      date_created(base::Time::Now()),
      last_modified(base::Time::Now()),
      last_visited(base::Time()),
      created_by_policy(false),
      usage_count(0),
      prepopulate_id(0),
      sync_guid(base::GenerateGUID()),
      keyword_(base::ASCIIToUTF16("dummy")),
      url_("x") {}

TemplateURLData::TemplateURLData(const TemplateURLData& other) = default;

TemplateURLData::TemplateURLData(const base::string16& name,
                                 const base::string16& keyword,
                                 base::StringPiece search_url,
                                 base::StringPiece suggest_url,
                                 base::StringPiece image_url,
                                 base::StringPiece new_tab_url,
                                 base::StringPiece contextual_search_url,
                                 base::StringPiece logo_url,
                                 base::StringPiece doodle_url,
                                 base::StringPiece search_url_post_params,
                                 base::StringPiece suggest_url_post_params,
                                 base::StringPiece image_url_post_params,
                                 base::StringPiece favicon_url,
                                 base::StringPiece encoding,
                                 const base::ListValue& alternate_urls_list,
                                 int prepopulate_id)
    : suggestions_url(suggest_url),
      image_url(image_url),
      new_tab_url(new_tab_url),
      contextual_search_url(contextual_search_url),
      logo_url(logo_url),
      doodle_url(doodle_url),
      search_url_post_params(search_url_post_params),
      suggestions_url_post_params(suggest_url_post_params),
      image_url_post_params(image_url_post_params),
      favicon_url(favicon_url),
      safe_for_autoreplace(true),
      id(0),
      date_created(base::Time()),
      last_modified(base::Time()),
      created_by_policy(false),
      usage_count(0),
      prepopulate_id(prepopulate_id),
      sync_guid(base::GenerateGUID()) {
  SetShortName(name);
  SetKeyword(keyword);
  SetURL(search_url.as_string());
  input_encodings.push_back(encoding.as_string());
  for (size_t i = 0; i < alternate_urls_list.GetSize(); ++i) {
    std::string alternate_url;
    alternate_urls_list.GetString(i, &alternate_url);
    DCHECK(!alternate_url.empty());
    alternate_urls.push_back(alternate_url);
  }
}

TemplateURLData::~TemplateURLData() {
}

void TemplateURLData::SetShortName(const base::string16& short_name) {
  // Remove tabs, carriage returns, and the like, as they can corrupt
  // how the short name is displayed.
  short_name_ = base::CollapseWhitespace(short_name, true);
}

void TemplateURLData::SetKeyword(const base::string16& keyword) {
  DCHECK(!keyword.empty());

  // Case sensitive keyword matching is confusing. As such, we force all
  // keywords to be lower case.
  keyword_ = base::i18n::ToLower(keyword);
}

void TemplateURLData::SetURL(const std::string& url) {
  DCHECK(!url.empty());
  url_ = url;
}
