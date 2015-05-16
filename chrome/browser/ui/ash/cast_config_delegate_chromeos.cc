// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/cast_config_delegate_chromeos.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_api.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

namespace chromeos {
namespace {

using JavaScriptResultCallback =
    content::RenderFrameHost::JavaScriptResultCallback;

// Returns the cast extension if it exists.
const extensions::Extension* FindCastExtension() {
  // TODO(jdufault): Figure out how to correctly handle multiprofile mode.
  // See crbug.com/488751
  Profile* profile = ProfileManager::GetActiveUserProfile();
  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::ExtensionSet& enabled_extensions =
      extension_registry->enabled_extensions();

  for (size_t i = 0; i < arraysize(extensions::kChromecastExtensionIds); ++i) {
    const std::string extension_id(extensions::kChromecastExtensionIds[i]);
    if (enabled_extensions.Contains(extension_id)) {
      return extension_registry->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::ENABLED);
    }
  }
  return nullptr;
}

// Utility method that returns the currently active RenderViewHost.
content::RenderViewHost* GetRenderViewHost() {
  const extensions::Extension* extension = FindCastExtension();
  if (!extension)
    return nullptr;
  // TODO(jdufault): Figure out how to correctly handle multiprofile mode.
  // See crbug.com/488751
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return nullptr;
  extensions::ProcessManager* pm = extensions::ProcessManager::Get(profile);
  return pm->GetBackgroundHostForExtension(extension->id())->render_view_host();
}

// Executes JavaScript in the context of the cast extension's background page.
void ExecuteJavaScript(const std::string& javascript) {
  auto rvh = GetRenderViewHost();
  if (!rvh)
    return;
  rvh->GetMainFrame()->ExecuteJavaScript(base::UTF8ToUTF16(javascript));
}

// Executes JavaScript in the context of the cast extension's background page.
// Invokes |callback| with the return value of the invoked javascript.
void ExecuteJavaScriptWithCallback(const std::string& javascript,
                                   const JavaScriptResultCallback& callback) {
  auto rvh = GetRenderViewHost();
  if (!rvh)
    return;
  rvh->GetMainFrame()->ExecuteJavaScript(base::UTF8ToUTF16(javascript),
                                         callback);
}

// Handler for GetReceiversAndActivities.
void GetReceiversAndActivitiesCallback(
    const ash::CastConfigDelegate::ReceiversAndActivitesCallback& callback,
    const base::Value* value) {
  ash::CastConfigDelegate::ReceiversAndActivites receiver_activites;
  const base::ListValue* ra_list = nullptr;
  if (value->GetAsList(&ra_list)) {
    for (auto i = ra_list->begin(); i != ra_list->end(); ++i) {
      const base::DictionaryValue* ra_dict = nullptr;
      if ((*i)->GetAsDictionary(&ra_dict)) {
        const base::DictionaryValue* receiver_dict(nullptr),
            *activity_dict(nullptr);
        ash::CastConfigDelegate::ReceiverAndActivity receiver_activity;
        if (ra_dict->GetDictionary("receiver", &receiver_dict)) {
          receiver_dict->GetString("name", &receiver_activity.receiver.name);
          receiver_dict->GetString("id", &receiver_activity.receiver.id);
        }
        if (ra_dict->GetDictionary("activity", &activity_dict) &&
            !activity_dict->empty()) {
          activity_dict->GetString("id", &receiver_activity.activity.id);
          activity_dict->GetString("title", &receiver_activity.activity.title);
          activity_dict->GetString("activityType",
                                   &receiver_activity.activity.activity_type);
          activity_dict->GetBoolean("allowStop",
                                    &receiver_activity.activity.allow_stop);
          activity_dict->GetInteger("tabId",
                                    &receiver_activity.activity.tab_id);
        }
        receiver_activites[receiver_activity.receiver.id] = receiver_activity;
      }
    }
  }
  callback.Run(receiver_activites);
}

}  // namespace

CastConfigDelegateChromeos::CastConfigDelegateChromeos() {
}

CastConfigDelegateChromeos::~CastConfigDelegateChromeos() {
}

bool CastConfigDelegateChromeos::HasCastExtension() const {
  return FindCastExtension() != nullptr;
}

void CastConfigDelegateChromeos::GetReceiversAndActivities(
    const ReceiversAndActivitesCallback& callback) {
  ExecuteJavaScriptWithCallback(
      "backgroundSetup.getMirrorCapableReceiversAndActivities();",
      base::Bind(&GetReceiversAndActivitiesCallback, callback));
}

void CastConfigDelegateChromeos::CastToReceiver(
    const std::string& receiver_id) {
  ExecuteJavaScript("backgroundSetup.launchDesktopMirroring('" + receiver_id +
                    "');");
}

void CastConfigDelegateChromeos::StopCasting(const std::string& activity_id) {
  ExecuteJavaScript("backgroundSetup.stopCastMirroring('user-stop');");
}

void CastConfigDelegateChromeos::LaunchCastOptions() {
  chrome::NavigateParams params(
      ProfileManager::GetActiveUserProfile(),
      FindCastExtension()->GetResourceURL("options.html"),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
}

}  // namespace chromeos
