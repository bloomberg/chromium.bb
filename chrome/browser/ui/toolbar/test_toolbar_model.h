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
  virtual string16 GetText(bool allow_search_term_replacement) const OVERRIDE;
  virtual string16 GetCorpusNameForMobile() const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool WouldPerformSearchTermReplacement(
      bool ignore_editing) const OVERRIDE;
  virtual SecurityLevel GetSecurityLevel(bool ignore_editing) const OVERRIDE;
  virtual int GetIcon() const OVERRIDE;
  virtual string16 GetEVCertName() const OVERRIDE;
  virtual bool ShouldDisplayURL() const OVERRIDE;

  void set_text(const string16& text) { text_ = text; }
  void set_url(const GURL& url) { url_ = url;}
  void set_replace_search_url_with_search_terms(bool should_replace_url) {
    should_replace_url_ = should_replace_url;
  }
  void set_security_level(SecurityLevel security_level) {
    security_level_ = security_level;
  }
  void set_icon(int icon) { icon_ = icon; }
  void set_ev_cert_name(const string16& ev_cert_name) {
    ev_cert_name_ = ev_cert_name;
  }
  void set_should_display_url(bool should_display_url) {
    should_display_url_ = should_display_url;
  }

 private:
  string16 text_;
  GURL url_;
  bool should_replace_url_;
  SecurityLevel security_level_;
  int icon_;
  string16 ev_cert_name_;
  bool should_display_url_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_MODEL_H_
