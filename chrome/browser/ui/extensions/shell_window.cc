// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/shell_window.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/extensions/extension_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

namespace internal {

class ShellWindowController : public ExtensionWindowController {
 public:
  ShellWindowController(ShellWindow* shell_window, Profile* profile);

  // Overriden from ExtensionWindowController
  virtual int GetWindowId() const OVERRIDE;
  virtual std::string GetWindowTypeText() const OVERRIDE;
  virtual base::DictionaryValue* CreateWindowValue() const OVERRIDE;
  virtual base::DictionaryValue* CreateWindowValueWithTabs() const OVERRIDE;
  virtual bool CanClose(Reason* reason) const OVERRIDE;
  virtual void SetFullscreenMode(bool is_fullscreen,
                                 const GURL& extension_url) const OVERRIDE;

 private:
  ShellWindow* shell_window_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowController);
};

ShellWindowController::ShellWindowController(
    ShellWindow* shell_window,
    Profile* profile)
    : ExtensionWindowController(shell_window, profile),
      shell_window_(shell_window) {
}

int ShellWindowController::GetWindowId() const {
  return static_cast<int>(shell_window_->session_id().id());
}

namespace keys = extension_tabs_module_constants;

std::string ShellWindowController::GetWindowTypeText() const {
  return keys::kWindowTypeValueShell;
}

base::DictionaryValue* ShellWindowController::CreateWindowValue() const {
  DictionaryValue* result = ExtensionWindowController::CreateWindowValue();
  return result;
}

base::DictionaryValue* ShellWindowController::CreateWindowValueWithTabs()
    const {
  return CreateWindowValue();
}

bool ShellWindowController::CanClose(Reason* reason) const {
  return true;
}

void ShellWindowController::SetFullscreenMode(bool is_fullscreen,
                                              const GURL& extension_url) const {
  // TODO(mihaip): implement
}

}  // namespace internal

ShellWindow* ShellWindow::Create(Profile* profile,
                                 const Extension* extension,
                                 const GURL& url) {
  ExtensionProcessManager* manager = profile->GetExtensionProcessManager();
  DCHECK(manager);

  // This object will delete itself when the window is closed.
  return ShellWindow::CreateShellWindow(
      manager->CreateShellHost(extension, url));
}

void ShellWindow::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      if (content::Details<ExtensionHost>(host_.get()) == details)
        Close();
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* unloaded_extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      // We compare extension IDs and not Extension pointers since ExtensionHost
      // nulls out its Extension pointer when it gets this notification.
      if (host_->extension_id() == unloaded_extension->id())
        Close();
      break;
    }
    case content::NOTIFICATION_APP_TERMINATING:
      Close();
      break;
    default:
      NOTREACHED() << "Received unexpected notification";
  }
}

ShellWindow::ShellWindow(ExtensionHost* host)
    : host_(host) {
  // Close the window in response to window.close() and the like.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(host->profile()));
  // Also close if the window if the extension has been unloaded (parallels
  // NOTIFICATION_EXTENSION_UNLOADED closing the app's tabs in TabStripModel).
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(host->profile()));
  // Close when the browser is exiting.
  // TODO(mihaip): we probably don't want this in the long run (when platform
  // apps are no longer tied to the browser process).
  registrar_.Add(this, content::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  // Prevent the browser process from shutting down while this window is open.
  BrowserList::StartKeepAlive();

  // Make this window available to the extension API.
  extension_window_controller_.reset(
      new internal::ShellWindowController(this, host->profile()));
}

ShellWindow::~ShellWindow() {
  // Unregister now to prevent getting NOTIFICATION_APP_TERMINATING if we're the
  // last window open.
  registrar_.RemoveAll();

  // Remove shutdown prevention.
  BrowserList::EndKeepAlive();
}
