// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CSP_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_CSP_PARSER_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"

class Profile;

namespace extensions {

class CSPParser : public ProfileKeyedAPI {
 public:
  explicit CSPParser(Profile* profile);
  virtual ~CSPParser();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<CSPParser>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<CSPParser>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "CSPParser";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  DISALLOW_COPY_AND_ASSIGN(CSPParser);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CSP_PARSER_H_
