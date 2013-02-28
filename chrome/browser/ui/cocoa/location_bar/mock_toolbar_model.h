// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_MOCK_TOOLBAR_MODEL_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_MOCK_TOOLBAR_MODEL_H_

#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "base/compiler_specific.h"

namespace chrome {
namespace testing {

// Override the toolbar model to allow |GetInputInProgress| and
// |WouldReplaceSearchURLWithSearchTerms| to be customized.
class MockToolbarModel : public ToolbarModel {
 public:
  MockToolbarModel();
  virtual ~MockToolbarModel();

  virtual string16 GetText(
      bool display_search_urls_as_search_terms) const OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;
  virtual bool WouldReplaceSearchURLWithSearchTerms() const OVERRIDE;
  virtual SecurityLevel GetSecurityLevel() const OVERRIDE;
  virtual int GetIcon() const OVERRIDE;
  virtual string16 GetEVCertName() const OVERRIDE;
  virtual bool ShouldDisplayURL() const OVERRIDE;
  virtual void SetInputInProgress(bool value) OVERRIDE;
  virtual bool GetInputInProgress() const OVERRIDE;

  void set_would_replace_search_url_with_search_terms(bool value) {
    would_replace_search_url_with_search_terms_ = value;
  }

 private:
  bool input_in_progress_;
  bool would_replace_search_url_with_search_terms_;
};

}  // namespace testing
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_MOCK_TOOLBAR_MODEL_H_
