// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVE_SCRIPT_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVE_SCRIPT_CONTROLLER_H_

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace IPC {
class Message;
}

class ExtensionAction;

namespace extensions {
class Extension;

// The provider for ExtensionActions corresponding to scripts which are actively
// running or need permission.
// TODO(rdevlin.cronin): This isn't really a controller, but it has good parity
// with PageAction"Controller".
class ActiveScriptController : public LocationBarController::ActionProvider,
                               public content::WebContentsObserver {
 public:
  explicit ActiveScriptController(content::WebContents* web_contents);
  virtual ~ActiveScriptController();

  // Notify the ActiveScriptController that an extension is running a script.
  // TODO(rdevlin.cronin): Soon, this should be ask the user for permission,
  // rather than simply notifying them.
  void NotifyScriptExecuting(const std::string& extension_id, int page_id);

  // Notifies the ActiveScriptController of detected ad injection.
  void OnAdInjectionDetected(const std::vector<std::string> ad_injectors);

  // LocationBarControllerProvider implementation.
  virtual ExtensionAction* GetActionForExtension(
      const Extension* extension) OVERRIDE;
  virtual LocationBarController::Action OnClicked(
      const Extension* extension) OVERRIDE;
  virtual void OnNavigated() OVERRIDE;

 private:
  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Handle the NotifyExtensionScriptExecution message.
  void OnNotifyExtensionScriptExecution(const std::string& extension_id,
                                        int page_id);

  // Log metrics.
  void LogUMA() const;

  // Whether or not the ActiveScriptController is enabled (corresponding to the
  // kActiveScriptEnforcement switch). If it is not, it acts as an empty shell,
  // always allowing scripts to run and never displaying actions.
  bool enabled_;

  // The extensions that have called ExecuteScript on the current frame.
  std::set<std::string> extensions_executing_scripts_;

  // The extensions which have injected ads.
  std::set<std::string> ad_injectors_;

  // Script badges that have been generated for extensions. This is both those
  // with actions already declared that are copied and normalised, and actions
  // that get generated for extensions that haven't declared anything.
  typedef std::map<std::string, linked_ptr<ExtensionAction> > ActiveScriptMap;
  ActiveScriptMap active_script_actions_;

  DISALLOW_COPY_AND_ASSIGN(ActiveScriptController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVE_SCRIPT_CONTROLLER_H_
