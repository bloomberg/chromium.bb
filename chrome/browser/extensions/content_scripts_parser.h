// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CONTENT_SCRIPTS_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_CONTENT_SCRIPTS_PARSER_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class Profile;

namespace extensions {

// The profile-keyed service that manages the content scripts API.
class ContentScriptsParser : public ProfileKeyedAPI {
 public:
  explicit ContentScriptsParser(Profile* profile);
  virtual ~ContentScriptsParser();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<ContentScriptsParser>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<ContentScriptsParser>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "ContentScriptsParser";
  }

  DISALLOW_COPY_AND_ASSIGN(ContentScriptsParser);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CONTENT_SCRIPTS_PARSER_H_
