// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_START_PAGE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_START_PAGE_SERVICE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/web_contents.h"

namespace extensions {
class Extension;
}

class Profile;

namespace app_list {

class RecommendedApps;

// StartPageService collects data to be displayed in app list's start page
// and hosts the start page contents.
class StartPageService : public BrowserContextKeyedService {
 public:
  typedef std::vector<scoped_refptr<const extensions::Extension> >
      ExtensionList;
  // Gets the instance for the given profile.
  static StartPageService* Get(Profile* profile);

  content::WebContents* contents() { return contents_.get(); }
  RecommendedApps* recommended_apps() { return recommended_apps_.get(); }

 private:
  // A BrowserContextKeyedServiceFactory for this service.
  class Factory;

  // ProfileDestroyObserver to shutdown the service on exiting. WebContents
  // depends on the profile and needs to be closed before the profile and its
  // keyed service shutdown.
  class ProfileDestroyObserver;

  explicit StartPageService(Profile* profile);
  virtual ~StartPageService();

  // BrowserContextKeyedService overrides:
  virtual void Shutdown() OVERRIDE;

  Profile* profile_;
  scoped_ptr<content::WebContents> contents_;
  scoped_ptr<ProfileDestroyObserver> profile_destroy_observer_;
  scoped_ptr<RecommendedApps> recommended_apps_;

  DISALLOW_COPY_AND_ASSIGN(StartPageService);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_START_PAGE_SERVICE_H_
