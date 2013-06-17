// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNDO_UNDO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UNDO_UNDO_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class Profile;
class UndoService;

// Singleton that owns all UndoService and associates them with Profiles.
class UndoServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static UndoService* GetForProfile(Profile* profile);

  static UndoServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<UndoServiceFactory>;

  UndoServiceFactory();
  virtual ~UndoServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(UndoServiceFactory);
};

#endif  // CHROME_BROWSER_UNDO_UNDO_SERVICE_FACTORY_H_
