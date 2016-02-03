// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IMPL_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IMPL_H_

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/toolbar/toolbar_model.h"
#include "url/gurl.h"

class ToolbarModelDelegate;

namespace content {
class NavigationController;
class WebContents;
}

namespace net {
class X509Certificate;
}

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
class ToolbarModelImpl : public ToolbarModel {
 public:
  explicit ToolbarModelImpl(ToolbarModelDelegate* delegate);
  ~ToolbarModelImpl() override;

 private:
  // ToolbarModel:
  base::string16 GetText() const override;
  base::string16 GetFormattedURL(size_t* prefix_end) const override;
  base::string16 GetCorpusNameForMobile() const override;
  GURL GetURL() const override;
  bool WouldPerformSearchTermReplacement(bool ignore_editing) const override;
  security_state::SecurityStateModel::SecurityLevel GetSecurityLevel(
      bool ignore_editing) const override;
  int GetIcon() const override;
  gfx::VectorIconId GetVectorIcon() const override;
  base::string16 GetEVCertName() const override;
  bool ShouldDisplayURL() const override;

  // Returns the navigation controller used to retrieve the navigation entry
  // from which the states are retrieved.
  // If this returns NULL, default values are used.
  content::NavigationController* GetNavigationController() const;

  // Returns search terms as in search::GetSearchTerms() if such terms should
  // appear in the omnibox (i.e. the page is sufficiently secure, search term
  // replacement is enabled, editing is not in progress, etc.).  If
  // |ignore_editing| is true, the "editing not in progress" check is skipped.
  base::string16 GetSearchTerms(bool ignore_editing) const;

  ToolbarModelDelegate* delegate_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarModelImpl);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IMPL_H_
