// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/start_page_service.h"

#include <string>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/recommended_apps.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace app_list {

class StartPageService::Factory : public BrowserContextKeyedServiceFactory {
 public:
  static StartPageService* GetForProfile(Profile* profile) {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kShowAppListStartPage)) {
      return NULL;
    }

    return static_cast<StartPageService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static Factory* GetInstance() {
    return Singleton<Factory>::get();
  }

 private:
  friend struct DefaultSingletonTraits<Factory>;

  Factory()
      : BrowserContextKeyedServiceFactory(
            "AppListStartPageService",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(extensions::ExtensionSystemFactory::GetInstance());
    DependsOn(extensions::InstallTrackerFactory::GetInstance());
  }

  virtual ~Factory() {}

  // BrowserContextKeyedServiceFactory overrides:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE {
     Profile* profile = static_cast<Profile*>(context);
     return new StartPageService(profile);
  }

  DISALLOW_COPY_AND_ASSIGN(Factory);
};

class StartPageService::ProfileDestroyObserver
    : public content::NotificationObserver {
 public:
  explicit ProfileDestroyObserver(StartPageService* service)
      : service_(service) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_PROFILE_DESTROYED,
                   content::NotificationService::AllSources());
  }
  virtual ~ProfileDestroyObserver() {}

 private:
  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(chrome::NOTIFICATION_PROFILE_DESTROYED, type);
    service_->Shutdown();
  }

  StartPageService* service_;  // Owner of this class.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDestroyObserver);
};

class StartPageService::StartPageWebContentsDelegate
    : public content::WebContentsDelegate {
 public:
  StartPageWebContentsDelegate() {}
  virtual ~StartPageWebContentsDelegate() {}

  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE {
    if (MediaStreamInfoBarDelegate::Create(web_contents, request, callback))
      NOTREACHED() << "Media stream not allowed for WebUI";
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StartPageWebContentsDelegate);
};

// static
StartPageService* StartPageService::Get(Profile* profile) {
  return Factory::GetForProfile(profile);
}

StartPageService::StartPageService(Profile* profile)
    : profile_(profile),
      profile_destroy_observer_(new ProfileDestroyObserver(this)),
      recommended_apps_(new RecommendedApps(profile)) {
  contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(profile_)));
  contents_delegate_.reset(new StartPageWebContentsDelegate());
  contents_->SetDelegate(contents_delegate_.get());

  GURL url(chrome::kChromeUIAppListStartPageURL);
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAppListStartPageURL)) {
    url = GURL(
        command_line->GetSwitchValueASCII(switches::kAppListStartPageURL));
  }

  contents_->GetController().LoadURL(
      url,
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
}

StartPageService::~StartPageService() {}

void StartPageService::Shutdown() {
  contents_.reset();
}

}  // namespace app_list
