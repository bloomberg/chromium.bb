// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEB_INTENTS_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_WEB_INTENTS_PARSER_H_

#include "base/basictypes.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"

class Profile;

namespace extensions {

class WebIntentsParser : public ProfileKeyedAPI {
 public:
  explicit WebIntentsParser(Profile* profile);
  virtual ~WebIntentsParser();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<WebIntentsParser>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<WebIntentsParser>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "WebIntentsParser";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  DISALLOW_COPY_AND_ASSIGN(WebIntentsParser);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEB_INTENTS_PARSER_H_
