// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/action_box_button_controller.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_to_mobile_service.h"
#include "chrome/browser/extensions/api/page_launcher/page_launcher_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"


using content::UserMetricsAction;
using content::WebContents;
using extensions::ActionInfo;

void ActionBoxButtonController::Delegate::ShowMenu(
    scoped_ptr<ActionBoxMenuModel> menu_model) {
}

ActionBoxButtonController::ActionBoxButtonController(Browser* browser,
                                                     Delegate* delegate)
    : browser_(browser),
      delegate_(delegate),
      next_command_id_(0) {
  DCHECK(browser_);
  DCHECK(delegate_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(browser->profile()));
}

ActionBoxButtonController::~ActionBoxButtonController() {}

scoped_ptr<ActionBoxMenuModel> ActionBoxButtonController::CreateMenuModel() {
  // Build a menu model and display the menu.
  scoped_ptr<ActionBoxMenuModel> menu_model(
      new ActionBoxMenuModel(browser_->profile(), this));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // In some unit tests, GetActiveWebContents can return NULL.
  bool starred = browser_->tab_strip_model()->GetActiveWebContents() &&
      BookmarkTabHelper::FromWebContents(browser_->tab_strip_model()->
          GetActiveWebContents())->is_starred();
  menu_model->AddItemWithStringId(
      IDC_BOOKMARK_PAGE_FROM_STAR,
      starred ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR);
  menu_model->SetIcon(
      menu_model->GetIndexOfCommandId(IDC_BOOKMARK_PAGE_FROM_STAR),
      rb.GetNativeImageNamed(starred ? IDR_STAR_LIT : IDR_STAR));

  // TODO(msw): Show the item as disabled for chrome: and file: scheme pages?
  if (ChromeToMobileService::UpdateAndGetCommandState(browser_)) {
    menu_model->AddItemWithStringId(IDC_CHROME_TO_MOBILE_PAGE,
                                    IDS_CHROME_TO_MOBILE_BUBBLE_TOOLTIP);
    menu_model->SetIcon(
        menu_model->GetIndexOfCommandId(IDC_CHROME_TO_MOBILE_PAGE),
        rb.GetNativeImageNamed(IDR_MOBILE));
  }

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser_->profile())->
          extension_service();
  if (extension_service) {
    const ExtensionSet* extensions = extension_service->extensions();
    for (ExtensionSet::const_iterator it = extensions->begin();
         it != extensions->end(); ++it) {
      const extensions::Extension* extension = *it;
      if (ActionInfo::GetPageLauncherInfo(extension)) {
        int command_id = GetCommandIdForExtension(*extension);
        menu_model->AddExtension(*extension, command_id);
      }
    }
  }
  return menu_model.Pass();
}

void ActionBoxButtonController::OnButtonClicked() {
  content::RecordAction(UserMetricsAction("ActionBox.ClickButton"));
  delegate_->ShowMenu(CreateMenuModel());
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

void ActionBoxButtonController::ExecuteCommand(int command_id,
                                               int event_flags) {
  // If the command id belongs to an extension, dispatch an onClicked event
  // to its pageLauncher.
  ExtensionIdCommandMap::const_iterator it =
      extension_command_ids_.find(command_id);
  if (it != extension_command_ids_.end()) {
    WebContents* web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();

    std::string page_title = UTF16ToUTF8(web_contents->GetTitle());
    std::string selected_text =
        UTF16ToUTF8(web_contents->GetRenderWidgetHostView()->GetSelectedText());
    extensions::PageLauncherAPI::DispatchOnClickedEvent(
        browser_->profile(),
        it->second,
        web_contents->GetURL(),
        web_contents->GetContentsMimeType(),
        page_title.empty() ? NULL : &page_title,
        selected_text.empty() ? NULL : &selected_text);
    return;
  }

  // Otherwise, let the browser handle the command.
  chrome::ExecuteCommand(browser_, command_id);
}

int ActionBoxButtonController::GetCommandIdForExtension(
    const extensions::Extension& extension) {
  for (ExtensionIdCommandMap::const_iterator it =
       extension_command_ids_.begin();
       it != extension_command_ids_.end(); ++it) {
    if (it->second == extension.id())
      return it->first;
  }

  int command_id = GetNextCommandId();
  extension_command_ids_[command_id] = extension.id();

  return command_id;
}

int ActionBoxButtonController::GetNextCommandId() {
  int command_id = next_command_id_;
  // Find an available command id to return next time the function is called.
  do {
    next_command_id_++;
    // Larger command ids are reserved for non-dynamic entries, so we start
    // reusing old ids at this point.
    if (next_command_id_ >= IDC_MinimumLabelValue)
      next_command_id_ = 0;
  } while (extension_command_ids_.find(next_command_id_) !=
           extension_command_ids_.end());

  return command_id;
}

void ActionBoxButtonController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED);
  const extensions::Extension* extension =
      content::Details<extensions::UnloadedExtensionInfo>(details)->extension;

  // Remove any entry point command ids associated with the extension.
  for (ExtensionIdCommandMap::iterator it = extension_command_ids_.begin();
       it != extension_command_ids_.end();) {
    if (it->second== extension->id())
      extension_command_ids_.erase(it++);
    else
      ++it;
  }
  // TODO(kalman): if there's a menu open, remove it from that too.
  // We may also want to listen to EXTENSION_LOADED to do the opposite.
}
