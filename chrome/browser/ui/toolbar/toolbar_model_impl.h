// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IMPL_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "googleurl/src/gurl.h"

class Profile;
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
  virtual ~ToolbarModelImpl();

  static SecurityLevel GetSecurityLevelForWebContents(
      content::WebContents* web_contents);

  // Overriden from ToolbarModel.
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

  // Returns "<organization_name> [<country>]".
  static string16 GetEVCertName(const net::X509Certificate& cert);

 private:
  // Returns the navigation controller used to retrieve the navigation entry
  // from which the states are retrieved.
  // If this returns NULL, default values are used.
  content::NavigationController* GetNavigationController() const;

  // Helper method to extract the profile from the navigation controller.
  Profile* GetProfile() const;

  // Returns search terms as in chrome::search::GetSearchTerms unless those
  // terms would be treated by the omnibox as a navigation.
  string16 GetSearchTerms() const;

  ToolbarModelDelegate* delegate_;

  // Whether the text in the location bar is currently being edited.
  bool input_in_progress_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarModelImpl);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_IMPL_H_
