// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEB_ACCESSIBLE_RESOURCES_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_WEB_ACCESSIBLE_RESOURCES_PARSER_H_

#include "base/basictypes.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"

class Profile;

namespace extensions {

class WebAccessibleResourcesParser : public ProfileKeyedAPI {
 public:
  explicit WebAccessibleResourcesParser(Profile* profile);
  virtual ~WebAccessibleResourcesParser();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<WebAccessibleResourcesParser>*
      GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<WebAccessibleResourcesParser>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "WebAccessibleResourcesParser";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  DISALLOW_COPY_AND_ASSIGN(WebAccessibleResourcesParser);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEB_ACCESSIBLE_RESOURCES_PARSER_H_
