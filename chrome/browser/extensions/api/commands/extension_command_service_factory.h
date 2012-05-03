// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COMMANDS_EXTENSION_COMMAND_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_COMMANDS_EXTENSION_COMMAND_SERVICE_FACTORY_H_
#pragma once

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ExtensionCommandService;

// Singleton that associate ExtensionCommandService objects with Profiles.
class ExtensionCommandServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ExtensionCommandService* GetForProfile(Profile* profile);

  static ExtensionCommandServiceFactory* GetInstance();

  // Overridden from ProfileKeyedBaseFactory:
  virtual bool ServiceIsCreatedWithProfile() OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ExtensionCommandServiceFactory>;

  ExtensionCommandServiceFactory();
  virtual ~ExtensionCommandServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCommandServiceFactory);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_COMMANDS_EXTENSION_COMMAND_SERVICE_FACTORY_H_
