// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefChangeRegistrar;
class PrefService;
class Profile;

class TranslateAcceptLanguages : public content::NotificationObserver {
 public:
  TranslateAcceptLanguages();
  virtual ~TranslateAcceptLanguages();

  // Returns true if |language| is available as Accept-Languages. |language|
  // will be cnverted if it has the synonym of accept language.
  static bool CanBeAcceptLanguage(const std::string& language);

  // Returns true if the passed language has been configured by the user as an
  // accept language. |language| will be converted if it has the synonym of
  // accept languages.
  bool IsAcceptLanguage(Profile* profile,
                        const std::string& language);

 private:
  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Initializes the |accept_languages_| language table based on the associated
  // preference in |prefs|.
  void InitAcceptLanguages(PrefService* prefs);

  // A map that associates a profile with its parsed "accept languages".
  typedef std::set<std::string> LanguageSet;
  typedef std::map<PrefService*, LanguageSet> PrefServiceLanguagesMap;
  PrefServiceLanguagesMap accept_languages_;

  // Each PrefChangeRegistrar only tracks a single PrefService, so a map from
  // each PrefService used to its registrar is needed.
  typedef std::map<PrefService*, PrefChangeRegistrar*> PrefServiceRegistrarMap;
  PrefServiceRegistrarMap pref_change_registrars_;

  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TranslateAcceptLanguages);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_H_
