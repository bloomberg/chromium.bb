// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_EXTENSION_APP_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_EXTENSION_APP_PROVIDER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/window_open_disposition.h"

// This provider is responsible for keeping track of which Extension Apps are
// installed and their URLs.  An instance of it gets created and managed by
// AutocompleteController.
class ExtensionAppProvider : public AutocompleteProvider,
                             public content::NotificationObserver {
 public:
  ExtensionAppProvider(AutocompleteProviderListener* listener,
                       Profile* profile);

  // AutocompleteProvider:
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;

  // Launch an Extension App from |match| details provided by the Omnibox. If
  // the application wants to launch as a window or panel, |disposition| is
  // ignored; otherwise it's used to determine in which tab we'll launch the
  // application.
  static void LaunchAppFromOmnibox(const AutocompleteMatch& match,
                                   Profile* profile,
                                   WindowOpenDisposition disposition);

 private:
  friend class ExtensionAppProviderTest;
  FRIEND_TEST_ALL_PREFIXES(ExtensionAppProviderTest, CreateMatchSanitize);

  // ExtensionApp stores the minimal metadata that we need to match against
  // eligible apps.
  struct ExtensionApp {
    // App's name.
    string16 name;
    // App's launch URL (for platform apps, which don't have a launch URL, this
    // just points to the app's origin).
    string16 launch_url;
    // If false, then the launch_url will not be considered for matching,
    // not shown next to the match, and not displayed as the editable text if
    // the user selects the match with the arrow keys.
    bool should_match_against_launch_url;
  };
  typedef std::vector<ExtensionApp> ExtensionApps;

  virtual ~ExtensionAppProvider();

  void AddExtensionAppForTesting(const ExtensionApp& extension_app);

  // Construct a match for the specified parameters.
  AutocompleteMatch CreateAutocompleteMatch(const AutocompleteInput& input,
                                            const ExtensionApp& app,
                                            size_t name_match_index,
                                            size_t url_match_index);

  // Fetch the current app list and cache it locally.
  void RefreshAppList();

  // Calculate the relevance of the match.
  int CalculateRelevance(AutocompleteInput::Type type,
                         int input_length,
                         int target_length,
                         const GURL& url);

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  // Our cache of ExtensionApp objects (name + url) representing the extension
  // apps we know/care about.
  ExtensionApps extension_apps_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_EXTENSION_APP_PROVIDER_H_
