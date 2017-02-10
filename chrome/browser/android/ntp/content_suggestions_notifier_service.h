// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_CONTENT_SUGGESTIONS_NOTIFIER_SERVICE_H_
#define CHROME_BROWSER_ANDROID_NTP_CONTENT_SUGGESTIONS_NOTIFIER_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ntp_snippets {
class ContentSuggestionsService;
}  // namespace ntp_snippets

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class ContentSuggestionsNotifierService : public KeyedService {
 public:
  ContentSuggestionsNotifierService(
      Profile* profile,
      ntp_snippets::ContentSuggestionsService* suggestions);

  ~ContentSuggestionsNotifierService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  class NotifyingObserver;
  std::unique_ptr<NotifyingObserver> observer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentSuggestionsNotifierService);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_CONTENT_SUGGESTIONS_NOTIFIER_SERVICE_H_
