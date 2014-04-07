// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/ui/webui/mojo_web_ui_handler.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"

class AutocompleteController;
class Profile;

// Implementation of OmniboxUIHandlerMojo.  StartOmniboxQuery() calls to a
// private AutocompleteController. It also listens for updates from the
// AutocompleteController to OnResultChanged() and passes those results to
// the OmniboxPage.
class OmniboxUIHandler : public AutocompleteControllerDelegate,
                         public OmniboxUIHandlerMojo,
                         public MojoWebUIHandler {
 public:
  OmniboxUIHandler(ScopedOmniboxPageHandle handle, Profile* profile);
  virtual ~OmniboxUIHandler();

  // AutocompleteControllerDelegate overrides:
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

  // OmniboxUIHandlerMojo overrides:
  virtual void StartOmniboxQuery(const mojo::String& input_string,
                                 int32_t cursor_position,
                                 bool prevent_inline_autocomplete,
                                 bool prefer_keyword,
                                 int32_t page_classification) OVERRIDE;

 private:
  // Looks up whether the hostname is a typed host (i.e., has received
  // typed visits).  Return true if the lookup succeeded; if so, the
  // value of |is_typed_host| is set appropriately.
  bool LookupIsTypedHost(const base::string16& host, bool* is_typed_host) const;

  // Re-initializes the AutocompleteController in preparation for the
  // next query.
  void ResetController();

  mojo::RemotePtr<OmniboxPage> page_;

  // The omnibox AutocompleteController that collects/sorts/dup-
  // eliminates the results as they come in.
  scoped_ptr<AutocompleteController> controller_;

  // Time the user's input was sent to the omnibox to start searching.
  // Needed because we also pass timing information in the object we
  // hand back to the javascript.
  base::Time time_omnibox_started_;

  // The Profile* handed to us in our constructor.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_OMNIBOX_UI_HANDLER_H_
