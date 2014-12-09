// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_HOTWORD_AUDIO_HISTORY_HANDLER_H_
#define CHROME_BROWSER_SEARCH_HOTWORD_AUDIO_HISTORY_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "content/public/browser/browser_context.h"

class Profile;

namespace history {
class WebHistoryService;
}

// A class which handles the audio history pref for hotwording. This has been
// pulled into its own class in order to transparently (to the rest of
// hotwording) handle changing user global pref management systems.
class HotwordAudioHistoryHandler {
 public:
  typedef base::Callback<void(bool success, bool new_enabled_value)>
      HotwordAudioHistoryCallback;

  explicit HotwordAudioHistoryHandler(content::BrowserContext* context);
  virtual ~HotwordAudioHistoryHandler();

  // Updates the current preference value based on the user's account info
  // or false if the user is not signed in.
  void GetAudioHistoryEnabled(const HotwordAudioHistoryCallback& callback);

  // Sets the user's global pref value for enabling audio history.
  void SetAudioHistoryEnabled(const bool enabled,
                              const HotwordAudioHistoryCallback& callback);

  // This helper function is made public for testing.
  virtual history::WebHistoryService* GetWebHistory();

 private:
  // Called upon completion of web history->GetAudioHistoryEnabled.
  void GetAudioHistoryComplete(
      const HotwordAudioHistoryCallback& callback,
      bool success,
      bool new_enabled_value);

  // Called upon completion of web history->SetAudioHistoryEnabled.
  // |new_enabled_value| is the desired value of the preference.
  // |callback_enabled_value| should not be considered valid.
  void SetAudioHistoryComplete(
      bool new_enabled_value,
      const HotwordAudioHistoryCallback& callback,
      bool success,
      bool callback_enabled_value);

  Profile* profile_;

  base::WeakPtrFactory<HotwordAudioHistoryHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HotwordAudioHistoryHandler);
};

#endif  // CHROME_BROWSER_SEARCH_HOTWORD_AUDIO_HISTORY_HANDLER_H_
