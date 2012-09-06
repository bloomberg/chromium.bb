// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMAND_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMAND_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {
class CommandService;
}

namespace extensions {

// Singleton that associate CommandService objects with Profiles.
class CommandServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static CommandService* GetForProfile(Profile* profile);

  static CommandServiceFactory* GetInstance();

  // Overridden from ProfileKeyedBaseFactory:
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<CommandServiceFactory>;

  CommandServiceFactory();
  virtual ~CommandServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CommandServiceFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COMMANDS_COMMAND_SERVICE_FACTORY_H_
