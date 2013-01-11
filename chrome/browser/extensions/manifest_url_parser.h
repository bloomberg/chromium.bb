// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MANIFEST_URL_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_MANIFEST_URL_PARSER_H_

#include "base/basictypes.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"

class Profile;

namespace extensions {

class ManifestURLParser : public ProfileKeyedAPI {
 public:
  explicit ManifestURLParser(Profile* profile);
  virtual ~ManifestURLParser();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<ManifestURLParser>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<ManifestURLParser>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "ManifestURLParser";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  DISALLOW_COPY_AND_ASSIGN(ManifestURLParser);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MANIFEST_URL_PARSER_H_
