// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/test_toolbar_model.h"

#include "grit/theme_resources.h"

TestToolbarModel::TestToolbarModel()
    : ToolbarModel(),
      search_terms_type_(NO_SEARCH_TERMS),
      security_level_(NONE),
      icon_(IDR_LOCATION_BAR_HTTP),
      should_display_url_(true),
      input_in_progress_(false) {}

TestToolbarModel::~TestToolbarModel() {}

string16 TestToolbarModel::GetText(
    bool display_search_urls_as_search_terms) const {
  return text_;
}

string16 TestToolbarModel::GetCorpusNameForMobile() const {
  return string16();
}

GURL TestToolbarModel::GetURL() const {
  return url_;
}

ToolbarModel::SearchTermsType TestToolbarModel::GetSearchTermsType() const {
  return search_terms_type_;
}

void TestToolbarModel::SetSupportsExtractionOfURLLikeSearchTerms(bool value) {
}

ToolbarModel::SecurityLevel TestToolbarModel::GetSecurityLevel() const {
  return security_level_;
}

int TestToolbarModel::GetIcon() const {
  return icon_;
}

string16 TestToolbarModel::GetEVCertName() const {
  return ev_cert_name_;
}

bool TestToolbarModel::ShouldDisplayURL() const {
  return should_display_url_;
}

void TestToolbarModel::SetInputInProgress(bool value) {
  input_in_progress_ = value;
}

bool TestToolbarModel::GetInputInProgress() const {
  return input_in_progress_;
}
