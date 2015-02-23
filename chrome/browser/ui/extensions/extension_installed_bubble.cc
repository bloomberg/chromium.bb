// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_installed_bubble.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "chrome/common/extensions/command.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::Extension;

namespace {

// How long to wait for browser action animations to complete before retrying.
const int kAnimationWaitMs = 50;
// How often we retry when waiting for browser action animation to end.
const int kAnimationWaitRetries = 10;

// Returns the keybinding for an extension command, or a null if none exists.
scoped_ptr<extensions::Command> GetCommand(
    const std::string& extension_id,
    Profile* profile,
    ExtensionInstalledBubble::BubbleType type) {
  scoped_ptr<extensions::Command> result;
  extensions::Command command;
  extensions::CommandService* command_service =
      extensions::CommandService::Get(profile);
  bool has_command = false;
  if (type == ExtensionInstalledBubble::BROWSER_ACTION) {
    has_command = command_service->GetBrowserActionCommand(
        extension_id, extensions::CommandService::ACTIVE, &command, nullptr);
  } else if (type == ExtensionInstalledBubble::PAGE_ACTION) {
    has_command = command_service->GetPageActionCommand(
        extension_id, extensions::CommandService::ACTIVE, &command, nullptr);
  }
  if (has_command)
    result.reset(new extensions::Command(command));
  return result.Pass();
}

} // namespace

ExtensionInstalledBubble::ExtensionInstalledBubble(Delegate* delegate,
                                                   const Extension* extension,
                                                   Browser* browser,
                                                   const SkBitmap& icon)
    : delegate_(delegate),
      extension_(extension),
      browser_(browser),
      icon_(icon),
      extension_registry_observer_(this),
      animation_wait_retries_(0),
      weak_factory_(this) {
  if (!extensions::OmniboxInfo::GetKeyword(extension).empty())
    type_ = OMNIBOX_KEYWORD;
  else if (extensions::ActionInfo::GetBrowserActionInfo(extension))
    type_ = BROWSER_ACTION;
  else if (extensions::ActionInfo::GetPageActionInfo(extension) &&
           extensions::ActionInfo::IsVerboseInstallMessage(extension))
    type_ = PAGE_ACTION;
  else
    type_ = GENERIC;

  // |extension| has been initialized but not loaded at this point. We need
  // to wait on showing the Bubble until not only the EXTENSION_LOADED gets
  // fired, but all of the EXTENSION_LOADED Observers have run. Only then can we
  // be sure that a BrowserAction or PageAction has had views created which we
  // can inspect for the purpose of previewing of pointing to them.
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(browser->profile()));

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSING,
      content::Source<Browser>(browser));
}

ExtensionInstalledBubble::~ExtensionInstalledBubble() {}

void ExtensionInstalledBubble::IgnoreBrowserClosing() {
  registrar_.Remove(this, chrome::NOTIFICATION_BROWSER_CLOSING,
                    content::Source<Browser>(browser_));
}

base::string16 ExtensionInstalledBubble::GetHowToUseDescription() const {
  int message_id = 0;
  base::string16 extra;
  if (action_command_)
    extra = action_command_->accelerator().GetShortcutText();

  switch (type_) {
    case BROWSER_ACTION:
      message_id = extra.empty() ? IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO :
          IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO_WITH_SHORTCUT;
      break;
    case PAGE_ACTION:
      message_id = extra.empty() ? IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO :
          IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO_WITH_SHORTCUT;
      break;
    case OMNIBOX_KEYWORD:
      extra =
          base::UTF8ToUTF16(extensions::OmniboxInfo::GetKeyword(extension_));
      message_id = IDS_EXTENSION_INSTALLED_OMNIBOX_KEYWORD_INFO;
      break;
    case GENERIC:
      break;
  }

  if (message_id == 0)
    return base::string16();
  return extra.empty() ? l10n_util::GetStringUTF16(message_id) :
      l10n_util::GetStringFUTF16(message_id, extra);
}

void ExtensionInstalledBubble::ShowInternal() {
  if (delegate_->MaybeShowNow())
    return;
  if (animation_wait_retries_++ < kAnimationWaitRetries) {
    base::MessageLoopForUI::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ExtensionInstalledBubble::ShowInternal,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kAnimationWaitMs));
  }
}

void ExtensionInstalledBubble::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (extension == extension_) {
    // Parse the extension command, if one exists.
    action_command_ = GetCommand(extension_->id(), browser_->profile(), type_);

    animation_wait_retries_ = 0;
    // PostTask to ourself to allow all EXTENSION_LOADED Observers to run.
    base::MessageLoopForUI::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionInstalledBubble::ShowInternal,
                   weak_factory_.GetWeakPtr()));
  }
}

void ExtensionInstalledBubble::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (extension == extension_) {
    // Extension is going away, make sure ShowInternal won't be called.
    weak_factory_.InvalidateWeakPtrs();
    extension_ = NULL;
  }
}

void ExtensionInstalledBubble::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_BROWSER_CLOSING)
      << "Received unexpected notification";
  delete delegate_;
}
