// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_test_notification_observer.h"

#include "base/callback_list.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

using extensions::Extension;

namespace {

// A callback that returns true if the condition has been met and takes no
// arguments.
typedef base::Callback<bool(void)> ConditionCallback;

bool HasPageActionVisibilityReachedTarget(
    Browser* browser, size_t target_visible_page_action_count) {
  return extensions::extension_action_test_util::GetVisiblePageActionCount(
      browser->tab_strip_model()->GetActiveWebContents()) ==
      target_visible_page_action_count;
}

bool HaveAllExtensionRenderViewHostsFinishedLoading(
    extensions::ProcessManager* manager) {
  extensions::ProcessManager::ViewSet all_views = manager->GetAllViews();
  for (extensions::ProcessManager::ViewSet::const_iterator iter =
           all_views.begin();
       iter != all_views.end(); ++iter) {
    if ((*iter)->IsLoading())
      return false;
  }
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExtensionTestNotificationObserver::NotificationSet

class ExtensionTestNotificationObserver::NotificationSet
    : public content::NotificationObserver {
 public:
  void Add(int type, const content::NotificationSource& source);
  void Add(int type);

  // Notified any time an Add()ed notification is received.
  // The details of the notification are dropped.
  base::CallbackList<void()>& callback_list() {
    return callback_list_;
  }

 private:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar notification_registrar_;
  base::CallbackList<void()> callback_list_;
};

void ExtensionTestNotificationObserver::NotificationSet::Add(
    int type,
    const content::NotificationSource& source) {
  notification_registrar_.Add(this, type, source);
}

void ExtensionTestNotificationObserver::NotificationSet::Add(int type) {
  Add(type, content::NotificationService::AllSources());
}

void ExtensionTestNotificationObserver::NotificationSet::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  callback_list_.Notify();
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionTestNotificationObserver

ExtensionTestNotificationObserver::ExtensionTestNotificationObserver(
    Browser* browser)
    : browser_(browser),
      profile_(NULL),
      extension_installs_observed_(0),
      extension_load_errors_observed_(0),
      crx_installers_done_observed_(0) {
}

ExtensionTestNotificationObserver::~ExtensionTestNotificationObserver() {}

Profile* ExtensionTestNotificationObserver::GetProfile() {
  if (!profile_) {
    if (browser_)
      profile_ = browser_->profile();
    else
      profile_ = ProfileManager::GetActiveUserProfile();
  }
  return profile_;
}

void ExtensionTestNotificationObserver::WaitForNotification(
    int notification_type) {
  // TODO(bauerb): Using a WindowedNotificationObserver like this can break
  // easily, if the notification we're waiting for is sent before this method.
  // Change it so that the WindowedNotificationObserver is constructed earlier.
  content::NotificationRegistrar registrar;
  registrar.Add(
      this, notification_type, content::NotificationService::AllSources());
  content::WindowedNotificationObserver(
      notification_type, content::NotificationService::AllSources()).Wait();
}

bool ExtensionTestNotificationObserver::WaitForPageActionVisibilityChangeTo(
    int count) {
  extensions::ExtensionActionAPI::Get(GetProfile())->AddObserver(this);
  WaitForCondition(
      base::Bind(&HasPageActionVisibilityReachedTarget, browser_, count),
      NULL);
  extensions::ExtensionActionAPI::Get(GetProfile())->
      RemoveObserver(this);
  return true;
}

bool ExtensionTestNotificationObserver::WaitForExtensionViewsToLoad() {
  extensions::ProcessManager* manager =
      extensions::ExtensionSystem::Get(GetProfile())->process_manager();
  NotificationSet notification_set;
  notification_set.Add(content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
  notification_set.Add(content::NOTIFICATION_LOAD_STOP);
  WaitForCondition(
      base::Bind(&HaveAllExtensionRenderViewHostsFinishedLoading, manager),
      &notification_set);
  return true;
}

bool ExtensionTestNotificationObserver::WaitForExtensionInstall() {
  int before = extension_installs_observed_;
  WaitForNotification(
      extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED);
  return extension_installs_observed_ == (before + 1);
}

bool ExtensionTestNotificationObserver::WaitForExtensionInstallError() {
  int before = extension_installs_observed_;
  content::WindowedNotificationObserver(
      extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllSources()).Wait();
  return extension_installs_observed_ == before;
}

void ExtensionTestNotificationObserver::WaitForExtensionLoad() {
  WaitForNotification(extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED);
}

void ExtensionTestNotificationObserver::WaitForExtensionAndViewLoad() {
  this->WaitForExtensionLoad();
  WaitForExtensionViewsToLoad();
}

bool ExtensionTestNotificationObserver::WaitForExtensionLoadError() {
  int before = extension_load_errors_observed_;
  WaitForNotification(extensions::NOTIFICATION_EXTENSION_LOAD_ERROR);
  return extension_load_errors_observed_ != before;
}

bool ExtensionTestNotificationObserver::WaitForExtensionCrash(
    const std::string& extension_id) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      GetProfile())->extension_service();

  if (!service->GetExtensionById(extension_id, true)) {
    // The extension is already unloaded, presumably due to a crash.
    return true;
  }
  content::WindowedNotificationObserver(
      extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::NotificationService::AllSources()).Wait();
  return (service->GetExtensionById(extension_id, true) == NULL);
}

bool ExtensionTestNotificationObserver::WaitForCrxInstallerDone() {
  int before = crx_installers_done_observed_;
  WaitForNotification(extensions::NOTIFICATION_CRX_INSTALLER_DONE);
  return crx_installers_done_observed_ == (before + 1);
}

void ExtensionTestNotificationObserver::Watch(
    int type,
    const content::NotificationSource& source) {
  CHECK(!observer_);
  observer_.reset(new content::WindowedNotificationObserver(type, source));
  registrar_.Add(this, type, source);
}

void ExtensionTestNotificationObserver::Wait() {
  observer_->Wait();

  registrar_.RemoveAll();
  observer_.reset();
}

void ExtensionTestNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED:
      last_loaded_extension_id_ =
        content::Details<const Extension>(details).ptr()->id();
      VLOG(1) << "Got EXTENSION_LOADED notification.";
      break;

    case extensions::NOTIFICATION_CRX_INSTALLER_DONE:
      VLOG(1) << "Got CRX_INSTALLER_DONE notification.";
      {
          const Extension* extension =
            content::Details<const Extension>(details).ptr();
          if (extension)
            last_loaded_extension_id_ = extension->id();
          else
            last_loaded_extension_id_.clear();
      }
      ++crx_installers_done_observed_;
      break;

    case extensions::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED:
      VLOG(1) << "Got EXTENSION_INSTALLED notification.";
      ++extension_installs_observed_;
      break;

    case extensions::NOTIFICATION_EXTENSION_LOAD_ERROR:
      VLOG(1) << "Got EXTENSION_LOAD_ERROR notification.";
      ++extension_load_errors_observed_;
      break;

    default:
      NOTREACHED();
      break;
  }
}

void ExtensionTestNotificationObserver::OnPageActionsUpdated(
    content::WebContents* web_contents) {
  MaybeQuit();
}

void ExtensionTestNotificationObserver::WaitForCondition(
    const ConditionCallback& condition,
    NotificationSet* notification_set) {
  if (condition.Run())
    return;
  condition_ = condition;

  scoped_refptr<content::MessageLoopRunner> runner(
      new content::MessageLoopRunner);
  quit_closure_ = runner->QuitClosure();

  scoped_ptr<base::CallbackList<void()>::Subscription> subscription;
  if (notification_set) {
    subscription = notification_set->callback_list().Add(
        base::Bind(&ExtensionTestNotificationObserver::MaybeQuit,
                   base::Unretained(this)));
  }
  runner->Run();

  condition_.Reset();
  quit_closure_.Reset();
}

void ExtensionTestNotificationObserver::MaybeQuit() {
  if (condition_.Run())
    quit_closure_.Run();
}
