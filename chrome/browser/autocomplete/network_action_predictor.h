// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_NETWORK_ACTION_PREDICTOR_H_
#define CHROME_BROWSER_AUTOCOMPLETE_NETWORK_ACTION_PREDICTOR_H_
#pragma once

#include "base/string16.h"

struct AutocompleteMatch;
class Profile;

// This class is responsible for determining the correct predictive network
// action to take given for a given AutocompleteMatch and entered text.
class NetworkActionPredictor {
 public:
  enum Action {
    ACTION_PRERENDER = 0,
    ACTION_PRECONNECT,
    ACTION_NONE,
    LAST_PREDICT_ACTION = ACTION_NONE
  };

  explicit NetworkActionPredictor(Profile* profile);
  virtual ~NetworkActionPredictor();

  // Return the recommended action given |user_text|, the text the user has
  // entered in the Omnibox, and |match|, the suggestion from Autocomplete.
  // This method uses information from the ShortcutsBackend including how much
  // of the matching entry the user typed, and how long it's been since the user
  // visited the matching URL, to calculate a score between 0 and 1. This score
  // is then mapped to an Action.
  Action RecommendAction(const string16& user_text,
                         const AutocompleteMatch& match) const;

  // Return true if the suggestion type warrants a TCP/IP preconnection.
  // i.e., it is now quite likely that the user will select the related domain.
  static bool IsPreconnectable(const AutocompleteMatch& match);

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_NETWORK_ACTION_PREDICTOR_H_
