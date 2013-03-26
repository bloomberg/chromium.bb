// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_MANIFEST_PARSER_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_MANIFEST_PARSER_H_

#include "base/basictypes.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace extensions {

class ChromeManifestParser : public ProfileKeyedAPI,
                             public content::NotificationObserver {
 public:
  explicit ChromeManifestParser(Profile* profile);
  virtual ~ChromeManifestParser();

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<ChromeManifestParser>* GetFactoryInstance();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<ChromeManifestParser>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "ChromeManifestParser";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  Profile* const profile_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeManifestParser);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_MANIFEST_PARSER_H_
