// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_CHROME_TOOLBAR_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_CHROME_TOOLBAR_MODEL_DELEGATE_H_

#include "base/macros.h"
#include "chrome/browser/ui/toolbar/toolbar_model_delegate.h"

class Profile;

namespace content {
class NavigationEntry;
class NavigationController;
}

// Implementation of ToolbarModelDelegate for the Chrome embedder. It leaves out
// how to fetch the active WebContents to its subclasses.
class ChromeToolbarModelDelegate : public ToolbarModelDelegate {
 public:
  // Returns active WebContents.
  virtual content::WebContents* GetActiveWebContents() const = 0;

 protected:
  ChromeToolbarModelDelegate();
  ~ChromeToolbarModelDelegate() override;

 private:
  // ToolbarModelDelegate implementation:
  std::string GetAcceptLanguages() const override;
  base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url) const override;
  bool GetURL(GURL* url) const override;
  bool ShouldDisplayURL() const override;
  SecurityLevel GetSecurityLevel() const override;
  base::string16 GetSearchTerms(SecurityLevel security_level) const override;
  scoped_refptr<net::X509Certificate> GetCertificate() const override;

  // Returns the navigation controller used to retrieve the navigation entry
  // from which the states are retrieved. If this returns null, default values
  // are used.
  content::NavigationController* GetNavigationController() const;

  // Helper method to extract the navigation entry from the navigation
  // controller.
  content::NavigationEntry* GetNavigationEntry() const;

  // Helper method to extract the profile from the navigation controller.
  Profile* GetProfile() const;

  DISALLOW_COPY_AND_ASSIGN(ChromeToolbarModelDelegate);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_CHROME_TOOLBAR_MODEL_DELEGATE_H_
