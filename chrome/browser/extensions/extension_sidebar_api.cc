// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sidebar_api.h"

#include "base/json/json_writer.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sidebar/sidebar_container.h"
#include "chrome/browser/sidebar/sidebar_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {
// Errors.
const char kNoTabError[] = "No tab with id: *.";
const char kInvalidUrlError[] = "Invalid url: \"*\".";
const char kNoCurrentWindowError[] = "No current browser window was found";
const char kNoDefaultTabError[] = "No default tab was found";
const char kInvalidExpandContextError[] =
    "Sidebar can be expanded only in response to an explicit user gesture";
// Keys.
const char kBadgeTextKey[] = "text";
const char kImageDataKey[] = "imageData";
const char kStateKey[] = "state";
const char kTabIdKey[] = "tabId";
const char kTitleKey[] = "title";
const char kUrlKey[] = "url";
// Events.
const char kOnStateChanged[] = "experimental.sidebar.onStateChanged";
}  // namespace

namespace extension_sidebar_constants {
// Sidebar states.
const char kActiveState[] = "active";
const char kHiddenState[] = "hidden";
const char kShownState[] = "shown";
}

static GURL ResolvePossiblyRelativeURL(const std::string& url_string,
                                       Extension* extension) {
  GURL url = GURL(url_string);
  if (!url.is_valid())
    url = extension->GetResourceURL(url_string);

  return url;
}

static bool CanUseHost(Extension* extension,
                       const GURL& url,
                       std::string* error) {
  if (extension->HasHostPermission(url))
      return true;

  if (error) {
    *error = ExtensionErrorUtils::FormatErrorMessage(
        extension_manifest_errors::kCannotAccessPage, url.spec());
  }

  return false;
}


// static
void ExtensionSidebarEventRouter::OnStateChanged(
    Profile* profile, TabContents* tab, const std::string& content_id,
    const std::string& state) {
  int tab_id = ExtensionTabUtil::GetTabId(tab);
  DictionaryValue* details = new DictionaryValue;
  details->Set(kTabIdKey, Value::CreateIntegerValue(tab_id));
  details->Set(kStateKey, Value::CreateStringValue(state));

  ListValue args;
  args.Set(0, details);
  std::string json_args;
  base::JSONWriter::Write(&args, false, &json_args);

  const std::string& extension_id(content_id);
  profile->GetExtensionMessageService()->DispatchEventToExtension(
      extension_id, kOnStateChanged, json_args, profile, GURL());
}


bool SidebarFunction::RunImpl() {
  if (!args_.get())
    return false;

  DictionaryValue* details = NULL;
  DictionaryValue default_details;
  if (args_->empty()) {
    details = &default_details;
  } else {
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  }

  int tab_id;
  TabContents* tab_contents = NULL;
  if (details->HasKey(kTabIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetInteger(kTabIdKey, &tab_id));
    if (!ExtensionTabUtil::GetTabById(tab_id, profile(), include_incognito(),
                                      NULL, NULL, &tab_contents, NULL)) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          kNoTabError, base::IntToString(tab_id));
      return false;
    }
  } else {
    Browser* browser = GetCurrentBrowser();
    if (!browser) {
      error_ = kNoCurrentWindowError;
      return false;
    }
    if (!ExtensionTabUtil::GetDefaultTab(browser, &tab_contents, &tab_id)) {
      error_ = kNoDefaultTabError;
      return false;
    }
  }
  if (!tab_contents)
    return false;

  std::string content_id(GetExtension()->id());
  return RunImpl(tab_contents, content_id, *details);
}


bool CollapseSidebarFunction::RunImpl(TabContents* tab,
                                      const std::string& content_id,
                                      const DictionaryValue& details) {
  SidebarManager::GetInstance()->CollapseSidebar(tab, content_id);
  return true;
}

bool ExpandSidebarFunction::RunImpl(TabContents* tab,
                                    const std::string& content_id,
                                    const DictionaryValue& details) {
  // TODO(alekseys): enable this check back when WebKit's user gesture flag
  // reporting for extension calls is fixed.
  // if (!user_gesture()) {
  //  error_ = kInvalidExpandContextError;
  //  return false;
  // }
  SidebarManager::GetInstance()->ExpandSidebar(tab, content_id);
  return true;
}

bool GetStateSidebarFunction::RunImpl(TabContents* tab,
                                      const std::string& content_id,
                                      const DictionaryValue& details) {
  SidebarManager* manager = SidebarManager::GetInstance();

  const char* result = extension_sidebar_constants::kHiddenState;
  if (manager->GetSidebarTabContents(tab, content_id)) {
    bool is_active = false;
    // Sidebar is considered active only if tab is selected, sidebar UI
    // is expanded and this extension's content is displayed on it.
    SidebarContainer* active_sidebar =
        manager->GetActiveSidebarContainerFor(tab);
    // Check if sidebar UI is expanded and this extension's content
    // is displayed on it.
    if (active_sidebar && active_sidebar->content_id() == content_id) {
      if (!details.HasKey(kTabIdKey)) {
        is_active = NULL != GetCurrentBrowser();
      } else {
        int tab_id;
        EXTENSION_FUNCTION_VALIDATE(details.GetInteger(kTabIdKey, &tab_id));

        // Check if this tab is selected.
        Browser* browser = GetCurrentBrowser();
        TabContents* contents = NULL;
        int default_tab_id = -1;
        if (browser &&
            ExtensionTabUtil::GetDefaultTab(browser, &contents,
                                            &default_tab_id)) {
          is_active = default_tab_id == tab_id;
        }
      }
    }

    result = is_active ? extension_sidebar_constants::kActiveState :
                         extension_sidebar_constants::kShownState;
  }

  result_.reset(Value::CreateStringValue(result));
  return true;
}

bool HideSidebarFunction::RunImpl(TabContents* tab,
                                  const std::string& content_id,
                                  const DictionaryValue& details) {
  SidebarManager::GetInstance()->HideSidebar(tab, content_id);
  return true;
}

bool NavigateSidebarFunction::RunImpl(TabContents* tab,
                                      const std::string& content_id,
                                      const DictionaryValue& details) {
  std::string url_string;
  EXTENSION_FUNCTION_VALIDATE(details.GetString(kUrlKey, &url_string));
  GURL url = ResolvePossiblyRelativeURL(url_string, GetExtension());
  if (!url.is_valid()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kInvalidUrlError,
                                                     url_string);
    return false;
  }
  if (!url.SchemeIs(chrome::kExtensionScheme) &&
      !CanUseHost(GetExtension(), url, &error_)) {
    return false;
  }
  // Disallow requests outside of the requesting extension view's extension.
  if (url.SchemeIs(chrome::kExtensionScheme)) {
    std::string extension_id(url.host());
    if (extension_id != GetExtension()->id())
      return false;
  }

  SidebarManager::GetInstance()->NavigateSidebar(tab, content_id, GURL(url));
  return true;
}

bool SetBadgeTextSidebarFunction::RunImpl(TabContents* tab,
                                          const std::string& content_id,
                                          const DictionaryValue& details) {
  string16 badge_text;
  EXTENSION_FUNCTION_VALIDATE(details.GetString(kBadgeTextKey, &badge_text));
  SidebarManager::GetInstance()->SetSidebarBadgeText(
      tab, content_id, badge_text);
  return true;
}

bool SetIconSidebarFunction::RunImpl(TabContents* tab,
                                     const std::string& content_id,
                                     const DictionaryValue& details) {
  BinaryValue* binary;
  EXTENSION_FUNCTION_VALIDATE(details.GetBinary(kImageDataKey, &binary));
  IPC::Message bitmap_pickle(binary->GetBuffer(), binary->GetSize());
  void* iter = NULL;
  scoped_ptr<SkBitmap> bitmap(new SkBitmap);
  EXTENSION_FUNCTION_VALIDATE(
      IPC::ReadParam(&bitmap_pickle, &iter, bitmap.get()));
  SidebarManager::GetInstance()->SetSidebarIcon(tab, content_id, *bitmap);
  return true;
}

bool SetTitleSidebarFunction::RunImpl(TabContents* tab,
                                      const std::string& content_id,
                                      const DictionaryValue& details) {
  string16 title;
  EXTENSION_FUNCTION_VALIDATE(details.GetString(kTitleKey, &title));
  SidebarManager::GetInstance()->SetSidebarTitle(tab, content_id, title);
  return true;
}

bool ShowSidebarFunction::RunImpl(TabContents* tab,
                                  const std::string& content_id,
                                  const DictionaryValue& details) {
  SidebarManager::GetInstance()->ShowSidebar(tab, content_id);
  return true;
}
