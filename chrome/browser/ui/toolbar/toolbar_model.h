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

  virtual ~ToolbarModel();

  // Returns the text to be displayed in the toolbar for the current page.
  // The text is formatted in various ways:
  //   - If the current page's URL is a search URL for the user's default search
  //     engine, the query will be extracted and returned for display instead
  //     of the URL.
  //   - If the origin chip is enabled and visible, the text will be empty.
  //   - Otherwise, the text will contain the URL returned by GetFormattedURL().
  virtual base::string16 GetText() const = 0;

  // Returns a formatted URL for display in the toolbar. The formatting
  // includes:
  //   - Some characters may be unescaped.
  //   - The scheme and/or trailing slash may be dropped.
  // If |prefix_end| is non-NULL, it is set to the length of the pre-hostname
  // portion of the resulting URL.
  virtual base::string16 GetFormattedURL(size_t* prefix_end) const = 0;

  // Some search URLs bundle a special "corpus" param that we can extract and
  // display next to users' search terms in cases where we'd show the search
  // terms instead of the URL anyway.  For example, a Google image search might
  // show the corpus "Images:" plus a search string.  This is only used on
  // mobile.
  virtual base::string16 GetCorpusNameForMobile() const = 0;

  // Returns the URL of the current navigation entry.
  virtual GURL GetURL() const = 0;

  // Returns true if a call to GetText() would successfully replace the URL
  // with search terms.  If |ignore_editing| is true, the result reflects the
  // underlying state of the page without regard to any user edits that may be
  // in progress in the omnibox.
  virtual bool WouldPerformSearchTermReplacement(bool ignore_editing) const = 0;

  // Returns true if a call to GetText() would return something other than the
  // URL because of either search term replacement or URL omission in favor of
  // the origin chip.
  bool WouldReplaceURL() const;

  // Returns the security level that the toolbar should display.  If
  // |ignore_editing| is true, the result reflects the underlying state of the
  // page without regard to any user edits that may be in progress in the
  // omnibox.
  virtual SecurityLevel GetSecurityLevel(bool ignore_editing) const = 0;

  // Returns the resource_id of the icon to show to the left of the address,
  // based on the current URL.  When search term replacement is active, this
  // returns a search icon.  This doesn't cover specialized icons while the
  // user is editing; see OmniboxView::GetIcon().
  virtual int GetIcon() const = 0;

  // As |GetIcon()|, but returns the icon only taking into account the security
  // |level| given, ignoring search term replacement state.
  virtual int GetIconForSecurityLevel(SecurityLevel level) const = 0;

  // Returns the name of the EV cert holder.  This returns an empty string if
  // the security level is not EV_SECURE.
  virtual base::string16 GetEVCertName() const = 0;

  // Returns whether the URL for the current navigation entry should be
  // in the location bar.
  virtual bool ShouldDisplayURL() const = 0;

  // Returns true if a call to GetText() would return an empty string instead of
  // the URL that would have otherwise been displayed because the host/origin is
  // instead being displayed in the origin chip.  This returns false when we
  // wouldn't have displayed a URL to begin with (e.g. for the NTP).
  virtual bool WouldOmitURLDueToOriginChip() const = 0;

  // Returns true if the origin should be shown based on the current state of
  // the ToolbarModel.
  bool ShouldShowOriginChip() const;

  // Whether the text in the omnibox is currently being edited.
  void set_input_in_progress(bool input_in_progress) {
    input_in_progress_ = input_in_progress;
  }
  bool input_in_progress() const { return input_in_progress_; }

  // Whether the origin chip should be enabled.
  void set_origin_chip_enabled(bool enabled) {
    origin_chip_enabled_ = enabled;
  }
  bool origin_chip_enabled() const {
    return origin_chip_enabled_;
  }

  // Whether URL replacement should be enabled.
  void set_url_replacement_enabled(bool enabled) {
    url_replacement_enabled_ = enabled;
  }
  bool url_replacement_enabled() const {
    return url_replacement_enabled_;
  }

 protected:
  ToolbarModel();

 private:
  bool input_in_progress_;
  bool origin_chip_enabled_;
  bool url_replacement_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarModel);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_H_
