// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {
class InputMethodAPI;

class InputMethodAPIFactory : public ProfileKeyedServiceFactory {
 public:
  static InputMethodAPIFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<InputMethodAPIFactory>;

  InputMethodAPIFactory();
  virtual ~InputMethodAPIFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_FACTORY_H_
