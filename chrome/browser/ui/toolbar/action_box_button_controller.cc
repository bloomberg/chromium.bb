// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/action_box_button_controller.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "grit/generated_resources.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/webkit_glue.h"

namespace {

#if defined(ENABLE_WEB_INTENTS)
// This indicates we need to send UMA data about the number of
// "Share with X" commands shown in the menu after user tried to
// find share extensions from web store or the first use of action
// box after browser starts.
static bool send_uma_share_command_count = true;
#endif

// Share intents get command IDs that are beyond the maximal valid command ID
// (0xDFFF) so that they are not confused with actual commands that appear in
// the menu. Extensions get a reserved block of commands after share handlers.
// For more details see: chrome/app/chrome_command_ids.h
const int kMaxShareItemsToShow = 20;  // TODO(skare): Show extras in submenu.
enum ActionBoxLocalCommandIds {
  CWS_FIND_SHARE_INTENTS_COMMAND = 0xE000,
  SHARE_COMMAND_FIRST,
  SHARE_COMMAND_LAST =
      SHARE_COMMAND_FIRST + kMaxShareItemsToShow - 1,
  EXTENSION_COMMAND_FIRST
};

const char kShareIntentAction[] = "http://webintents.org/share";
const char kShareIntentMimeType[] = "text/uri-list";

}  // namespace

using content::UserMetricsAction;

ActionBoxButtonController::ActionBoxButtonController(Browser* browser,
                                                     Delegate* delegate)
    : browser_(browser),
      delegate_(delegate),
      next_extension_command_id_(EXTENSION_COMMAND_FIRST) {
  DCHECK(browser_);
  DCHECK(delegate_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(browser->profile()));
}

ActionBoxButtonController::~ActionBoxButtonController() {}

void ActionBoxButtonController::OnButtonClicked() {
  // Build a menu model and display the menu.
  scoped_ptr<ActionBoxMenuModel> menu_model(
      new ActionBoxMenuModel(browser_, this));

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser_->profile())->
          extension_service();

#if defined(ENABLE_WEB_INTENTS)
  // Add share intent triggers and a link to the web store.
  // Web Intents are not currently supported in Incognito mode.
  if (!browser_->profile()->IsOffTheRecord()) {
    int next_share_intent_command_id = SHARE_COMMAND_FIRST;
    share_intent_service_ids_.clear();
    const ExtensionSet* extension_set = extension_service->extensions();
    WebIntentsRegistry* intents_registry =
        WebIntentsRegistryFactory::GetForProfile(browser_->profile());
    for (ExtensionSet::const_iterator it = extension_set->begin();
         it != extension_set->end(); ++it) {
      const extensions::Extension* extension = *it;
      WebIntentsRegistry::IntentServiceList services;
      intents_registry->GetIntentServicesForExtensionFilter(
          ASCIIToUTF16(kShareIntentAction),
          ASCIIToUTF16(kShareIntentMimeType),
          extension->id(),
          &services);
      if (!services.empty()) {
        int command_id = next_share_intent_command_id++;
        if (command_id > SHARE_COMMAND_LAST)
          break;
        // TODO(skare): If an intent supports multiple services, be able to
        // disambiguate. Choosing the first matches the picker behavior; see
        // TODO in WebIntentPickerController::DispatchToInstalledExtension.
        share_intent_service_ids_[command_id] = services[0].service_url;
        menu_model->AddItem(command_id, services[0].title);
      }
    }

    // Add link to the Web Store to find additional share intents.
    menu_model->AddItemWithStringId(CWS_FIND_SHARE_INTENTS_COMMAND,
        IDS_FIND_SHARE_INTENTS);

    content::RecordAction(UserMetricsAction("ActionBox.ClickButton"));
    if (send_uma_share_command_count) {
      UMA_HISTOGRAM_ENUMERATION("ActionBox.ShareCommandCount",
          next_share_intent_command_id - SHARE_COMMAND_FIRST,
          kMaxShareItemsToShow + 1);
      send_uma_share_command_count = false;
    }
  }
#endif

  // Add Extensions.
  next_extension_command_id_ = EXTENSION_COMMAND_FIRST;
  extension_command_ids_.clear();
  const extensions::ExtensionList& extensions =
      extension_service->toolbar_model()->action_box_menu_items();
  for (extensions::ExtensionList::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    menu_model->AddExtension(**it, GetCommandIdForExtension(**it));
  }

  // And show the menu.
  delegate_->ShowMenu(menu_model.Pass());
}

bool ActionBoxButtonController::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ActionBoxButtonController::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ActionBoxButtonController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void ActionBoxButtonController::ExecuteCommand(int command_id) {
  // Handle explicit intent triggers for share intent commands.
  if (share_intent_service_ids_.count(command_id) > 0) {
    TriggerExplicitShareIntent(share_intent_service_ids_[command_id]);
    return;
  }

#if defined(ENABLE_WEB_INTENTS)
  // Handle link to the CWS web store.
  if (command_id == CWS_FIND_SHARE_INTENTS_COMMAND) {
    NavigateToWebStoreShareIntentsList();
    return;
  }
#endif

  // Handle commands associated with extensions.
  // Note that the extension might have been uninstalled or disabled while the
  // menu was open (sync perhaps?) but that will just fall through safely.
  const extensions::Extension* extension =
      GetExtensionForCommandId(command_id);
  if (extension) {
    // TODO(kalman): do something with the result.
    extensions::ExtensionSystem::Get(browser_->profile())->
        extension_service()->toolbar_model()->ExecuteBrowserAction(
            extension, browser_, NULL);
    return;
  }

  // Otherwise, let the browser handle the command.
  chrome::ExecuteCommand(browser_, command_id);
}

int ActionBoxButtonController::GetCommandIdForExtension(
    const extensions::Extension& extension) {
  ExtensionIdCommandMap::iterator it =
      extension_command_ids_.find(extension.id());
  if (it != extension_command_ids_.end())
    return it->second;
  int command_id = next_extension_command_id_++;

  // Note that we deliberately don't clean up extension IDs here when
  // extensions are unloaded, so that if they're reloaded they get assigned the
  // old command ID. This situation could arise if an extension is updated
  // while the menu is open. On the other hand, we leak some memory... but
  // that's unlikely to matter.
  extension_command_ids_[extension.id()] = command_id;

  return command_id;
}

const extensions::Extension*
    ActionBoxButtonController::GetExtensionForCommandId(int command_id) {
  for (ExtensionIdCommandMap::iterator it = extension_command_ids_.begin();
       it != extension_command_ids_.end(); ++it) {
    if (it->second == command_id) {
      // Note: might be NULL anyway if the extension has been uninstalled.
      return extensions::ExtensionSystem::Get(browser_->profile())->
          extension_service()->extensions()->GetByID(it->first);
    }
  }

  return NULL;
}

void ActionBoxButtonController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED);
  const extensions::Extension* extension =
      content::Details<extensions::UnloadedExtensionInfo>(details)->extension;

  // TODO(kalman): if there's a menu open, remove it from that too.
  // We may also want to listen to EXTENSION_LOADED to do the opposite.
  extension_command_ids_.erase(extension->id());
}

void ActionBoxButtonController::TriggerExplicitShareIntent(
    const GURL& share_service_url) {
#if defined(ENABLE_WEB_INTENTS)
  const GURL& current_url =
      browser_->tab_strip_model()->GetActiveWebContents()->GetURL();
  webkit_glue::WebIntentData intent_data(
      ASCIIToUTF16(kShareIntentAction),
      ASCIIToUTF16(kShareIntentMimeType),
      UTF8ToUTF16(current_url.spec()));
  intent_data.service = share_service_url;
  static_cast<content::WebContentsDelegate*>(browser_)->WebIntentDispatch(
      NULL, content::WebIntentsDispatcher::Create(intent_data));
#endif
}

void ActionBoxButtonController::NavigateToWebStoreShareIntentsList() {
#if defined(ENABLE_WEB_INTENTS)
  const GURL& query_url = extension_urls::GetWebstoreIntentQueryURL(
      kShareIntentAction,
      kShareIntentMimeType);
  chrome::NavigateParams params(browser_->profile(), query_url,
                                content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);

  content::RecordAction(UserMetricsAction("ActionBox.FindShareHandlers"));
  send_uma_share_command_count = true;
#endif
}
