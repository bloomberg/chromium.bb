// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_PROFILE_PROVIDER_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_PROFILE_PROVIDER_H_
#pragma once

#include <string>
#include <vector>

// A user profile provider and state notification receiver for the
// SpellCheckHost.
class SpellCheckProfileProvider {
 public:
  typedef std::vector<std::string> CustomWordList;

  // Invoked on the UI thread when SpellCheckHost is initialized.
  virtual void SpellCheckHostInitialized(
      CustomWordList* custom_words) = 0;

  // Returns in-memory cache of custom word list.
  virtual const CustomWordList& GetCustomWords() const = 0;

  // Invoked on the Ui thread when new custom word is registered.
  virtual void CustomWordAddedLocally(const std::string& word) = 0;

 protected:
  virtual ~SpellCheckProfileProvider() {}
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_PROFILE_PROVIDER_H_
