// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_MODEL_H_

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"

// A ToolbarModel that is backed by instance variables, which are initialized
// with some basic values that can be changed with the provided setters. This
// should be used only for testing.
class TestToolbarModel : public ToolbarModel {
 public:
  TestToolbarModel();
  virtual ~TestToolbarModel();
  virtual base::string16 GetText() const OVERRIDE;
  virtual base::string16 GetFormattedURL(size_t* prefix_end) const OVERRIDE;
  virtual base::string16 GetCorpusNameForMobile() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool WouldPerformSearchTermReplacement(
      bool ignore_editing) const OVERRIDE;
  virtual SecurityLevel GetSecurityLevel(bool ignore_editing) const OVERRIDE;
  virtual int GetIcon() const OVERRIDE;
  virtual int GetIconForSecurityLevel(SecurityLevel level) const OVERRIDE;
  virtual base::string16 GetEVCertName() const OVERRIDE;
  virtual bool ShouldDisplayURL() const OVERRIDE;

  void set_text(const base::string16& text) { text_ = text; }
  void set_url(const GURL& url) { url_ = url;}
  void set_omit_url_due_to_origin_chip(bool omit_url_due_to_origin_chip) {
    omit_url_due_to_origin_chip_ = omit_url_due_to_origin_chip;
  }
  void set_perform_search_term_replacement(
      bool perform_search_term_replacement) {
    perform_search_term_replacement_ = perform_search_term_replacement;
  }
  void set_security_level(SecurityLevel security_level) {
    security_level_ = security_level;
  }
  void set_icon(int icon) { icon_ = icon; }
  void set_ev_cert_name(const base::string16& ev_cert_name) {
    ev_cert_name_ = ev_cert_name;
  }
  void set_should_display_url(bool should_display_url) {
    should_display_url_ = should_display_url;
  }

 private:
  virtual bool WouldOmitURLDueToOriginChip() const OVERRIDE;

  base::string16 text_;
  GURL url_;
  bool omit_url_due_to_origin_chip_;
  bool perform_search_term_replacement_;
  SecurityLevel security_level_;
  int icon_;
  base::string16 ev_cert_name_;
  bool should_display_url_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_MODEL_H_
