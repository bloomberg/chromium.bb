// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_INSTALLER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_INSTALLER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/extensions/webstore_startup_installer.h"
#include "content/public/browser/page_navigator.h"

class Profile;

namespace app_list {

// WebstoreInstaller handles install for web store search results.
class WebstoreInstaller : public extensions::WebstoreStartupInstaller,
                          public content::PageNavigator {
 public:
  typedef WebstoreStandaloneInstaller::Callback Callback;

  WebstoreInstaller(const std::string& webstore_item_id,
                    Profile* profile,
                    gfx::NativeWindow parent_window,
                    const Callback& callback);

 private:
  friend class base::RefCountedThreadSafe<WebstoreInstaller>;

  virtual ~WebstoreInstaller();

  // extensions::WebstoreStartupInstaller overrides:
  virtual scoped_ptr<ExtensionInstallPrompt> CreateInstallUI() OVERRIDE;

  // content::PageNavigator overrides:
  virtual content::WebContents* OpenURL(
      const content::OpenURLParams& params) OVERRIDE;

  Profile* profile_;
  gfx::NativeWindow parent_window_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreInstaller);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_INSTALLER_H_
