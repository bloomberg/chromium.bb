// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_OBSERVER_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_OBSERVER_H_
#pragma once

#include <string>
#include <vector>

// Observer for the SpellCheckHost.
//
// TODO(morrita): This class is no longer observer. So we need to
// rename this to SpellCheckProfile and existing SpellCheckProfile to
// SpellCheckProfileImpl.
class SpellCheckHostObserver {
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
  virtual ~SpellCheckHostObserver() {}
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_OBSERVER_H_
