// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class AutocompleteClassifier;
class Profile;

// Singleton that owns all AutocompleteClassifiers and associates them with
// Profiles.
class AutocompleteClassifierFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the AutocompleteClassifier for |profile|.
  static AutocompleteClassifier* GetForProfile(Profile* profile);

  static AutocompleteClassifierFactory* GetInstance();

  static KeyedService* BuildInstanceFor(content::BrowserContext* context);

 private:
  friend struct DefaultSingletonTraits<AutocompleteClassifierFactory>;

  AutocompleteClassifierFactory();
  virtual ~AutocompleteClassifierFactory();

  // BrowserContextKeyedServiceFactory:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteClassifierFactory);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_FACTORY_H_
