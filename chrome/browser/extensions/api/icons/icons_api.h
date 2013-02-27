// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ICONS_ICONS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ICONS_ICONS_API_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class Profile;

namespace extensions {

// The profile-keyed service that manages the extension icons API.
class IconsAPI : public ProfileKeyedAPI {
 public:
  explicit IconsAPI(Profile* profile);
  virtual ~IconsAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<IconsAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<IconsAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "IconsAPI";
  }

  DISALLOW_COPY_AND_ASSIGN(IconsAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ICONS_ICONS_API_H_
