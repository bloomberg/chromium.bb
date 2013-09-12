// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_H_

#include <string>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

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
  //   - if |allow_search_term_replacement| is true, the query will be extracted
  //     from search URLs for the user's default search engine and will be
  //     displayed in place of the URL.
  virtual string16 GetText(bool allow_search_term_replacement) const = 0;

  // Some search URLs bundle a special "corpus" param that we can extract and
  // display next to users' search terms in cases where we'd show the search
  // terms instead of the URL anyway.  For example, a Google image search might
  // show the corpus "Images:" plus a search string.  This is only used on
  // mobile.
  virtual string16 GetCorpusNameForMobile() const = 0;

  // Returns the URL of the current navigation entry.
  virtual GURL GetURL() const = 0;

  // Returns true if a call to GetText(true) would successfully replace the URL
  // with search terms.  If |ignore_editing| is true, the result reflects the
  // underlying state of the page without regard to any user edits that may be
  // in progress in the omnibox.
  virtual bool WouldPerformSearchTermReplacement(bool ignore_editing) const = 0;

  // Returns the security level that the toolbar should display.  If
  // |ignore_editing| is true, the result reflects the underlying state of the
  // page without regard to any user edits that may be in progress in the
  // omnibox.
  virtual SecurityLevel GetSecurityLevel(bool ignore_editing) const = 0;

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

  // Whether the text in the omnibox is currently being edited.
  void set_input_in_progress(bool input_in_progress) {
    input_in_progress_ = input_in_progress;
  }
  bool input_in_progress() const { return input_in_progress_; }

  // Whether search term replacement should be enabled.
  void set_search_term_replacement_enabled(bool enabled) {
    search_term_replacement_enabled_ = enabled;
  }
  bool search_term_replacement_enabled() const {
    return search_term_replacement_enabled_;
  }

 protected:
  ToolbarModel()
      : input_in_progress_(false),
        search_term_replacement_enabled_(true) {}

 private:
  bool input_in_progress_;
  bool search_term_replacement_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_H_
