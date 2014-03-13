// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_FACTORY_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class FindBarState;

class FindBarStateFactory : public BrowserContextKeyedServiceFactory {
 public:
  static FindBarState* GetForProfile(Profile* profile);

  // Retrieves the last prepopulate text for a given Profile.  If the profile is
  // incognito and has an empty prepopulate text, falls back to the
  // prepopulate text from the normal profile.
  static base::string16 GetLastPrepopulateText(Profile* profile);

  static FindBarStateFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<FindBarStateFactory>;

  FindBarStateFactory();
  virtual ~FindBarStateFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FindBarStateFactory);
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_STATE_FACTORY_H_
