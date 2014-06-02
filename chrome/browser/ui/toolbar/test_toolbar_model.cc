// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/test_toolbar_model.h"

#include "grit/theme_resources.h"

TestToolbarModel::TestToolbarModel()
    : ToolbarModel(),
      omit_url_due_to_origin_chip_(false),
      perform_search_term_replacement_(false),
      security_level_(NONE),
      icon_(IDR_LOCATION_BAR_HTTP),
      should_display_url_(true) {}

TestToolbarModel::~TestToolbarModel() {}

base::string16 TestToolbarModel::GetText() const {
  return text_;
}

base::string16 TestToolbarModel::GetFormattedURL(size_t* prefix_end) const {
  return text_;
}

base::string16 TestToolbarModel::GetCorpusNameForMobile() const {
  return base::string16();
}

GURL TestToolbarModel::GetURL() const {
  return url_;
}

bool TestToolbarModel::WouldOmitURLDueToOriginChip() const {
  return omit_url_due_to_origin_chip_;
}

bool TestToolbarModel::WouldPerformSearchTermReplacement(
    bool ignore_editing) const {
  return perform_search_term_replacement_;
}

ToolbarModel::SecurityLevel TestToolbarModel::GetSecurityLevel(
    bool ignore_editing) const {
  return security_level_;
}

int TestToolbarModel::GetIcon() const {
  return icon_;
}

int TestToolbarModel::GetIconForSecurityLevel(SecurityLevel level) const {
  return icon_;
}

base::string16 TestToolbarModel::GetEVCertName() const {
  return (security_level_ == EV_SECURE) ? ev_cert_name_ : base::string16();
}

bool TestToolbarModel::ShouldDisplayURL() const {
  return should_display_url_;
}
