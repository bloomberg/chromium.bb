// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_H_

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"

namespace net {
class X509Certificate;
}

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
class ToolbarModel {
 public:
  // TODO(wtc): unify ToolbarModel::SecurityLevel with SecurityStyle.  We
  // don't need two sets of security UI levels.  SECURITY_STYLE_AUTHENTICATED
  // needs to be refined into three levels: warning, standard, and EV.
  enum SecurityLevel {
#define DEFINE_TOOLBAR_MODEL_SECURITY_LEVEL(name,value)  name = value,
#include "chrome/browser/ui/toolbar/toolbar_model_security_level_list.h"
#undef DEFINE_TOOLBAR_MODEL_SECURITY_LEVEL
  };

  virtual ~ToolbarModel() {}

  // Returns the text for the current page's URL. This will have been formatted
  // for display to the user:
  //   - Some characters may be unescaped.
  //   - The scheme and/or trailing slash may be dropped.
  //   - if |display_search_urls_as_search_terms| is true, the query will be
  //   extracted from search URLs for the user's default search engine and those
  //   will be displayed in place of the URL.
  virtual string16 GetText(bool display_search_urls_as_search_terms) const = 0;

  // Returns the URL of the current navigation entry.
  virtual GURL GetURL() const = 0;

  // Returns true if a call to GetText(true) would successfully replace the URL
  // with search terms.
  virtual bool WouldReplaceSearchURLWithSearchTerms() const = 0;

  // Returns the security level that the toolbar should display.
  virtual SecurityLevel GetSecurityLevel() const = 0;

  // Returns the resource_id of the icon to show to the left of the address,
  // based on the current URL.  This doesn't cover specialized icons while the
  // user is editing; see OmniboxView::GetIcon().
  virtual int GetIcon() const = 0;

  // Returns the name of the EV cert holder.  Only call this when the security
  // level is EV_SECURE.
  virtual string16 GetEVCertName() const = 0;

  // Returns whether the URL for the current navigation entry should be
  // in the location bar.
  virtual bool ShouldDisplayURL() const = 0;

  // Getter/setter of whether the text in location bar is currently being
  // edited.
  virtual void SetInputInProgress(bool value) = 0;
  virtual bool GetInputInProgress() const = 0;

 protected:
   ToolbarModel() {}
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_H_
