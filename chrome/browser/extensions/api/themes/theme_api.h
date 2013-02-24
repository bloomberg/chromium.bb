// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_THEMES_THEME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_THEMES_THEME_API_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"

namespace extensions {

// TODO: Move the relevant code from chrome/browser/themes to this class.
class ThemeAPI : public ProfileKeyedAPI {
 public:
  explicit ThemeAPI(Profile* profile);
  virtual ~ThemeAPI();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<ThemeAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<ThemeAPI>;

  static const char* service_name() {
    return "ThemeAPI";
  }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_THEMES_THEME_API_H_
