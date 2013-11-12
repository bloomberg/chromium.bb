// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/drive_first_run_controller.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

// The initial time to wait in seconds before starting the opt-in.
const int kInitialDelaySeconds = 180;

// Time to wait for Drive app background page to come up before giving up.
const int kWebContentsTimeoutSeconds = 15;

// Google Drive offline opt-in endpoint.
const char kDriveOfflineEndpointUrl[] = "http://drive.google.com";

// Google Drive app id.
const char kDriveHostedAppId[] = "apdfllckaahabafndbhieahigkjlhalf";

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DriveWebContentsManager

// Manages web contents that does Google Drive offline opt-in. We create
// a background WebContents that loads a Drive endpoint to initialize offline
// mode. If successful, a background page will be opened to sync the user's
// files for offline use.
class DriveWebContentsManager : public content::WebContentsObserver,
                                public content::WebContentsDelegate,
                                public content::NotificationObserver {
 public:
  typedef base::Callback<void(bool)> CompletionCallback;

  DriveWebContentsManager(Profile* profile,
                          const CompletionCallback& completion_callback);
  virtual ~DriveWebContentsManager();

  // Start loading the WebContents for the endpoint in the context of the Drive
  // hosted app that will initialize offline mode and open a background page.
  void StartLoad();

  // Stop loading the endpoint. The |completion_callback| will not be called.
  void StopLoad();

 private:
  // Called when when offline initialization succeeds or fails and schedules
  // |RunCompletionCallback|.
  void OnOfflineInit(bool success);

  // Runs |completion_callback|.
  void RunCompletionCallback(bool success);

  // content::WebContentsObserver overrides:
  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      const string16& frame_unique_name,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const string16& error_description,
      content::RenderViewHost* render_view_host) OVERRIDE;

  virtual void DidFailLoad(int64 frame_id,
                           const GURL& validated_url,
                           bool is_main_frame,
                           int error_code,
                           const string16& error_description,
                           content::RenderViewHost* render_view_host) OVERRIDE;

  // content::WebContentsDelegate overrides:
  virtual bool ShouldCreateWebContents(
      content::WebContents* web_contents,
      int route_id,
      WindowContainerType window_container_type,
      const string16& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* profile_;
  scoped_ptr<content::WebContents> web_contents_;
  content::NotificationRegistrar registrar_;
  bool started_;
  CompletionCallback completion_callback_;
  base::WeakPtrFactory<DriveWebContentsManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DriveWebContentsManager);
};

DriveWebContentsManager::DriveWebContentsManager(
    Profile* profile,
    const CompletionCallback& completion_callback)
    : profile_(profile),
      started_(false),
      completion_callback_(completion_callback),
      weak_ptr_factory_(this) {
  DCHECK(!completion_callback_.is_null());
  registrar_.Add(this, chrome::NOTIFICATION_BACKGROUND_CONTENTS_OPENED,
                 content::Source<Profile>(profile_));
}

DriveWebContentsManager::~DriveWebContentsManager() {
}

void DriveWebContentsManager::StartLoad() {
  started_ = true;
  const GURL url(kDriveOfflineEndpointUrl);
  content::WebContents::CreateParams create_params(
        profile_, content::SiteInstance::CreateForURL(profile_, url));

  web_contents_.reset(content::WebContents::Create(create_params));
  web_contents_->SetDelegate(this);

  content::NavigationController::LoadURLParams load_params(url);
  load_params.transition_type = content::PAGE_TRANSITION_GENERATED;
  web_contents_->GetController().LoadURLWithParams(load_params);

  content::WebContentsObserver::Observe(web_contents_.get());
}

void DriveWebContentsManager::StopLoad() {
  started_ = false;
}

void DriveWebContentsManager::OnOfflineInit(bool success) {
  if (started_) {
    // We postpone notifying the controller as we may be in the middle
    // of a call stack for some routine of the contained WebContents.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&DriveWebContentsManager::RunCompletionCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   success));
    StopLoad();
  }
}

void DriveWebContentsManager::RunCompletionCallback(bool success) {
  completion_callback_.Run(success);
}

void DriveWebContentsManager::DidFailProvisionalLoad(
    int64 frame_id,
    const string16& frame_unique_name,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  OnOfflineInit(false);
}

void DriveWebContentsManager::DidFailLoad(
    int64 frame_id,
    const GURL& validated_url,
    bool is_main_frame,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  OnOfflineInit(false);
}

bool DriveWebContentsManager::ShouldCreateWebContents(
    content::WebContents* web_contents,
    int route_id,
    WindowContainerType window_container_type,
    const string16& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {

  if (window_container_type == WINDOW_CONTAINER_TYPE_NORMAL)
    return true;

  // Check that the target URL is for the Drive app.
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  const extensions::Extension *extension =
      service->GetInstalledApp(target_url);
  if (!extension || extension->id() != kDriveHostedAppId)
    return true;

  // The background contents creation is normally done in Browser, but
  // because we're using a detached WebContents, we need to do it ourselves.
  BackgroundContentsService* background_contents_service =
      BackgroundContentsServiceFactory::GetForProfile(profile_);

  // Prevent redirection if background contents already exists.
  if (background_contents_service->GetAppBackgroundContents(
      UTF8ToUTF16(kDriveHostedAppId))) {
    return false;
  }
  BackgroundContents* contents = background_contents_service
      ->CreateBackgroundContents(content::SiteInstance::Create(profile_),
                                 route_id,
                                 profile_,
                                 frame_name,
                                 ASCIIToUTF16(kDriveHostedAppId),
                                 partition_id,
                                 session_storage_namespace);

  contents->web_contents()->GetController().LoadURL(
      target_url,
      content::Referrer(),
      content::PAGE_TRANSITION_LINK,
      std::string());

  // Return false as we already created the WebContents here.
  return false;
}

void DriveWebContentsManager::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_BACKGROUND_CONTENTS_OPENED) {
    const std::string app_id = UTF16ToUTF8(
        content::Details<BackgroundContentsOpenedDetails>(details)
            ->application_id);
    if (app_id == kDriveHostedAppId)
      OnOfflineInit(true);
  }
}

////////////////////////////////////////////////////////////////////////////////
// DriveFirstRunController

DriveFirstRunController::DriveFirstRunController()
    : profile_(ProfileManager::GetDefaultProfile()),
      started_(false) {
}

DriveFirstRunController::~DriveFirstRunController() {
}

void DriveFirstRunController::EnableOfflineMode() {
  if (!started_) {
    started_ = true;
    initial_delay_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kInitialDelaySeconds),
      this,
      &DriveFirstRunController::EnableOfflineMode);
  }

  if (!UserManager::Get()->IsLoggedInAsRegularUser()) {
    LOG(ERROR) << "Attempting to enable offline access "
                  "but not logged in a regular user.";
    OnOfflineInit(false);
    return;
  }

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!extension_service->GetExtensionById(kDriveHostedAppId, false)) {
    LOG(WARNING) << "Drive app is not installed.";
    OnOfflineInit(false);
    return;
  }

  BackgroundContentsService* background_contents_service =
      BackgroundContentsServiceFactory::GetForProfile(profile_);
  if (background_contents_service->GetAppBackgroundContents(
      UTF8ToUTF16(kDriveHostedAppId))) {
    LOG(WARNING) << "Background page for Drive app already exists";
    OnOfflineInit(false);
    return;
  }

  web_contents_manager_.reset(new DriveWebContentsManager(
      profile_,
      base::Bind(&DriveFirstRunController::OnOfflineInit,
                 base::Unretained(this))));
  web_contents_manager_->StartLoad();
  web_contents_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kWebContentsTimeoutSeconds),
      this,
      &DriveFirstRunController::OnWebContentsTimedOut);
}

void DriveFirstRunController::OnWebContentsTimedOut() {
  LOG(WARNING) << "Timed out waiting for web contents to opt-in";
  OnOfflineInit(false);
}

void DriveFirstRunController::CleanUp() {
  if (web_contents_manager_)
    web_contents_manager_->StopLoad();
  web_contents_timer_.Stop();
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void DriveFirstRunController::OnOfflineInit(bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ash::Shell::GetInstance()->system_tray_notifier()
      ->NotifyDriveOfflineEnabled();
  CleanUp();
}

}  // namespace chromeos
