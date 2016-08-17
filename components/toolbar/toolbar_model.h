// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TOOLBAR_TOOLBAR_MODEL_H_
#define COMPONENTS_TOOLBAR_TOOLBAR_MODEL_H_

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/security_state/security_state_model.h"
#include "url/gurl.h"

namespace gfx {
enum class VectorIconId;
}

// This class is the model used by the toolbar, location bar and autocomplete
// edit.  It populates its states from the current navigation entry retrieved
// from the navigation controller returned by GetNavigationController().
class ToolbarModel {
 public:
  virtual ~ToolbarModel();

  // Returns a formatted URL for display in the toolbar. The formatting
  // includes:
  //   - Some characters may be unescaped.
  //   - The scheme and/or trailing slash may be dropped.
  // If |prefix_end| is non-NULL, it is set to the length of the pre-hostname
  // portion of the resulting URL.
  virtual base::string16 GetFormattedURL(size_t* prefix_end) const = 0;

  // Returns the URL of the current navigation entry.
  virtual GURL GetURL() const = 0;

  // Returns the security level that the toolbar should display.  If
  // |ignore_editing| is true, the result reflects the underlying state of the
  // page without regard to any user edits that may be in progress in the
  // omnibox.
  virtual security_state::SecurityStateModel::SecurityLevel GetSecurityLevel(
      bool ignore_editing) const = 0;

  // Returns the resource_id of the icon to show to the left of the address,
  // based on the current URL.  When search term replacement is active, this
  // returns a search icon.  This doesn't cover specialized icons while the
  // user is editing; see OmniboxView::GetIcon().
  virtual int GetIcon() const = 0;

  // Like GetIcon(), but gets the vector asset ID.
  virtual gfx::VectorIconId GetVectorIcon() const = 0;

  // Returns the name of the EV cert holder.  This returns an empty string if
  // the security level is not EV_SECURE.
  virtual base::string16 GetEVCertName() const = 0;

  // Returns whether the URL for the current navigation entry should be
  // in the location bar.
  virtual bool ShouldDisplayURL() const = 0;

  // Whether the text in the omnibox is currently being edited.
  void set_input_in_progress(bool input_in_progress) {
    input_in_progress_ = input_in_progress;
  }
  bool input_in_progress() const { return input_in_progress_; }

  // Whether URL replacement should be enabled.
  // TODO(treib,pkasting): Remove this. crbug.com/627747
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
  bool url_replacement_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarModel);
};

#endif  // COMPONENTS_TOOLBAR_TOOLBAR_MODEL_H_
