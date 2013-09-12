// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/test_toolbar_model.h"

#include "grit/theme_resources.h"

TestToolbarModel::TestToolbarModel()
    : ToolbarModel(),
      should_replace_url_(false),
      security_level_(NONE),
      icon_(IDR_LOCATION_BAR_HTTP),
      should_display_url_(true) {}

TestToolbarModel::~TestToolbarModel() {}

string16 TestToolbarModel::GetText(bool allow_search_term_replacement) const {
  return text_;
}

string16 TestToolbarModel::GetCorpusNameForMobile() const {
  return string16();
}

GURL TestToolbarModel::GetURL() const {
  return url_;
}

bool TestToolbarModel::WouldPerformSearchTermReplacement(
    bool ignore_editing) const {
  return should_replace_url_;
}

ToolbarModel::SecurityLevel TestToolbarModel::GetSecurityLevel(
    bool ignore_editing) const {
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
