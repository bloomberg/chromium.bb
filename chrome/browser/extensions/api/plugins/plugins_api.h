// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLUGINS_PLUGINS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLUGINS_PLUGINS_API_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class Profile;

namespace extensions {

// The profile-keyed service that manages the plugins for extensions.
class PluginsAPI : public ProfileKeyedAPI {
 public:
  explicit PluginsAPI(Profile* profile);
  virtual ~PluginsAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<PluginsAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<PluginsAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() { return "PluginsAPI"; }

  DISALLOW_COPY_AND_ASSIGN(PluginsAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLUGINS_PLUGINS_API_H_

