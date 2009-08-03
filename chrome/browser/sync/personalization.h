// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

// TODO(timsteele): Remove this file by finding proper homes for everything in
// trunk.
#ifndef CHROME_BROWSER_SYNC_PERSONALIZATION_H_
#define CHROME_BROWSER_SYNC_PERSONALIZATION_H_

#include <string>
#include "base/basictypes.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"

class Browser;
class DOMUI;
class DOMMessageHandler;
class Profile;
class RenderView;
class RenderViewHost;
class WebFrame;
class WebView;

class ProfileSyncService;
class ProfileSyncServiceObserver;

namespace views { class View; }

// TODO(ncarter): Move these switches into chrome_switches.  They are here
// now because we want to keep them secret during early development.
namespace switches {

extern const wchar_t kSyncServiceURL[];
extern const wchar_t kSyncServicePort[];
extern const wchar_t kSyncUserForTest[];
extern const wchar_t kSyncPasswordForTest[];

}

// Names of various preferences.
// TODO(munjal): Move these preferences to common/pref_names.h.
namespace prefs {
extern const wchar_t kSyncPath[];
extern const wchar_t kSyncLastSyncedTime[];
extern const wchar_t kSyncUserName[];
extern const wchar_t kSyncHasSetupCompleted[];
}

// Contains a profile sync service, which is initialized at profile creation.
// A pointer to this class is passed as a handle.
class ProfilePersonalization {
 public:
  ProfilePersonalization() {}
  virtual ~ProfilePersonalization() {}

  virtual ProfileSyncService* sync_service() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfilePersonalization);
};

// Contains methods to perform Personalization-related tasks on behalf of the
// caller.
namespace Personalization {

// Checks if P13N is globally disabled or not, and that |profile| has a valid
// ProfilePersonalization member (it can be NULL for TestingProfiles).
bool IsP13NDisabled(Profile* profile);

// Returns whether |url| should be loaded in a DOMUI.
bool NeedsDOMUI(const GURL& url);

// Construct a new ProfilePersonalization and return it so the caller can take
// ownership.
ProfilePersonalization* CreateProfilePersonalization(Profile* p);

// The caller of Create...() above should call this when the returned
// ProfilePersonalization object should be deleted.
void CleanupProfilePersonalization(ProfilePersonalization* p);

// Handler for "cloudy:stats"
std::string MakeCloudyStats();

// Construct a new DOMMessageHandler for the new tab page |dom_ui|.
DOMMessageHandler* CreateNewTabPageHandler(DOMUI* dom_ui);

// Get HTML for the Personalization iframe in the New Tab Page.
std::string GetNewTabSource();

// Returns the text for personalization info menu item and sets its enabled
// state.
std::wstring GetMenuItemInfoText(Browser* browser);

// Performs appropriate action when the sync menu item is clicked.
void HandleMenuItemClick(Profile* p);
}  // namespace Personalization

// The internal scheme used to retrieve HTML resources for personalization
// related code (e.g cloudy:stats, GAIA login page).
// We need to ensure the GAIA login HTML is loaded into an HTMLDialogContents.
// Outside of p13n (for the time being) only "gears://" gives this (see
// HtmlDialogContents::IsHtmlDialogUrl) for the application shortcut dialog.
// TODO(timsteele): We should have a robust way to handle this to allow more
// reuse of our HTML dialog code, perhaps by using a dedicated "dialog-resource"
// scheme (chrome-resource is coupled to DOM_UI). Figure out if that is the best
// course of action / pitch this idea to chromium-dev.
static const char kPersonalizationScheme[] = "cloudy";

#endif  // CHROME_BROWSER_SYNC_PERSONALIZATION_H_
#endif  // CHROME_PERSONALIZATION
