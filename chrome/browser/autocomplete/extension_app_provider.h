// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
//
// This file contains the Extension App autocomplete provider. The provider
// is responsible for keeping track of which Extension Apps are installed and
// their URLs.  An instance of it gets created and managed by the autocomplete
// controller.
//
// For more information on the autocomplete system in general, including how
// the autocomplete controller and autocomplete providers work, see
// chrome/browser/autocomplete.h.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_EXTENSION_APP_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_EXTENSION_APP_PROVIDER_H_
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class TemplateURLModel;

class ExtensionAppProvider : public AutocompleteProvider,
                             public NotificationObserver {
 public:
  ExtensionAppProvider(ACProviderListener* listener, Profile* profile);

  // Only used for testing.
  void AddExtensionAppForTesting(const std::string& app_name,
                                 const std::string url);

  // AutocompleteProvider implementation:
  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;

 private:
  // An ExtensionApp is a pair of Extension Name and the Launch URL.
  typedef std::pair<std::string, std::string> ExtensionApp;
  typedef std::vector<ExtensionApp> ExtensionApps;

  virtual ~ExtensionAppProvider();

  // Fetch the current app list and cache it locally.
  void RefreshAppList();

  // Register for install/uninstall notification so we can update our cache.
  void RegisterForNotifications();

  // Highlights a certain part of a match string within a certain match class.
  // |input| is the input we got from the user, |match_class| is the
  // AutoComplete match classification that keeps track of the highlighting
  // values, and |iter| is the location of the user input found within
  // |match_string|.
  void HighlightMatch(const AutocompleteInput& input,
                      ACMatchClassifications* match_class,
                      std::string::const_iterator iter,
                      const std::string& match_string);

  // Calculate the relevance of the match.
  int CalculateRelevance(AutocompleteInput::Type type,
                         int input_length,
                         int target_length,
                         const GURL& url);

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  // Our cache of ExtensionApp objects (name + url) representing the extension
  // apps we know about.
  ExtensionApps extension_apps_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_EXTENSION_APP_PROVIDER_H_
