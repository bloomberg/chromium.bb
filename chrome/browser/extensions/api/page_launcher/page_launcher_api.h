// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PAGE_LAUNCHER_PAGE_LAUNCHER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PAGE_LAUNCHER_PAGE_LAUNCHER_API_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"

class GURL;
class Profile;

namespace extensions {

class PageLauncherAPI : public ProfileKeyedAPI {
 public:
  explicit PageLauncherAPI(Profile* profile);
  virtual ~PageLauncherAPI();

  static void DispatchOnClickedEvent(Profile* profile,
                                     const std::string& extension_id,
                                     const GURL& url,
                                     const std::string& mimetype,
                                     const std::string* page_title,
                                     const std::string* selected_text);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<PageLauncherAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<PageLauncherAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "PageLauncherAPI";
  }

  DISALLOW_COPY_AND_ASSIGN(PageLauncherAPI);
};

}  // namespace extensions


#endif  // CHROME_BROWSER_EXTENSIONS_API_PAGE_LAUNCHER_PAGE_LAUNCHER_API_H_
