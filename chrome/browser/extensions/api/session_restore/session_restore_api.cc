// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/session_restore/session_restore_api.h"

#include <vector>

#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/tab_restore_service_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/browser/sync/glue/synced_session.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/layout.h"


namespace {

const unsigned int kMaxRecentlyClosedSessionResults = 25;
const char kRecentlyClosedListEmpty[] =
    "There are no recently closed sessions.";
const char kInvalidSessionId[] = "Invalid session id.";
const char kNoBrowserToRestoreSession[] =
    "There are no browser windows to restore the session.";

}  // namespace

namespace extensions {

namespace GetRecentlyClosed = api::session_restore::GetRecentlyClosed;
namespace Restore = api::session_restore::Restore;
namespace tabs = api::tabs;
namespace windows = api::windows;
namespace session_restore = api::session_restore;

scoped_ptr<tabs::Tab> SessionRestoreGetRecentlyClosedFunction::CreateTabModel(
    const TabRestoreService::Tab& tab, int selected_index) {
  scoped_ptr<tabs::Tab> tab_struct(new tabs::Tab);
  const TabNavigation& current_navigation =
      tab.navigations[tab.current_navigation_index];
  GURL gurl = current_navigation.virtual_url();
  std::string title = UTF16ToUTF8(current_navigation.title());

  tab_struct->url.reset(new std::string(gurl.spec()));
  tab_struct->title.reset(new std::string(title.empty() ? gurl.spec() : title));
  tab_struct->index = tab.tabstrip_index;
  tab_struct->pinned = tab.pinned;
  tab_struct->id = tab.id;
  tab_struct->window_id = tab.browser_id;
  tab_struct->index = tab.tabstrip_index;
  tab_struct->pinned = tab.pinned;
  tab_struct->selected = tab.tabstrip_index == selected_index;
  tab_struct->active = false;
  tab_struct->highlighted = false;
  tab_struct->incognito = false;
  ExtensionTabUtil::ScrubTabForExtension(GetExtension(),
                                         tab_struct.get());
  return tab_struct.Pass();
}

scoped_ptr<windows::Window>
    SessionRestoreGetRecentlyClosedFunction::CreateWindowModel(
        const TabRestoreService::Window& window) {
  scoped_ptr<windows::Window> window_struct(new windows::Window);
  DCHECK(!window.tabs.empty());

  scoped_ptr<std::vector<linked_ptr<tabs::Tab> > > tabs(
      new std::vector<linked_ptr<tabs::Tab> >);
  for (size_t i = 0; i < window.tabs.size(); ++i) {
    tabs->push_back(make_linked_ptr(CreateTabModel(window.tabs[i],
        window.selected_tab_index).release()));
  }
  window_struct->tabs.reset(tabs.release());
  window_struct->incognito = false;
  window_struct->always_on_top = false;
  window_struct->focused = false;
  window_struct->type = windows::Window::TYPE_NORMAL;
  window_struct->state = windows::Window::STATE_NORMAL;
  return window_struct.Pass();
}

scoped_ptr<session_restore::ClosedEntry>
    SessionRestoreGetRecentlyClosedFunction::CreateEntryModel(
        const TabRestoreService::Entry* entry) {
  scoped_ptr<session_restore::ClosedEntry> entry_struct(
      new session_restore::ClosedEntry);
  switch (entry->type) {
    case TabRestoreService::TAB:
      entry_struct->tab.reset(CreateTabModel(
          *static_cast<const TabRestoreService::Tab*>(entry), -1).release());
      break;
    case TabRestoreService::WINDOW:
      entry_struct->window.reset(CreateWindowModel(
          *static_cast<const TabRestoreService::Window*>(entry)).release());
      break;
    default:
      NOTREACHED();
  }
  entry_struct->timestamp = entry->timestamp.ToTimeT();
  entry_struct->id = entry->id;
  return entry_struct.Pass();
}

bool SessionRestoreGetRecentlyClosedFunction::RunImpl() {
  scoped_ptr<GetRecentlyClosed::Params> params(
      GetRecentlyClosed::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  unsigned int max_results = kMaxRecentlyClosedSessionResults;
  if (params->options && params->options->max_results)
    max_results = *params->options->max_results;
  EXTENSION_FUNCTION_VALIDATE(max_results >= 0 &&
                              max_results <= kMaxRecentlyClosedSessionResults);

  std::vector<linked_ptr<session_restore::ClosedEntry> > result;
  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  DCHECK(tab_restore_service);

  // List of entries. They are ordered from most to least recent.
  // We prune the list to contain max 25 entries at any time and removes
  // uninteresting entries.
  TabRestoreService::Entries entries = tab_restore_service->entries();
  for (TabRestoreService::Entries::const_iterator it = entries.begin();
       it != entries.end() && result.size() < max_results; ++it) {
    TabRestoreService::Entry* entry = *it;
    if (!params->options || params->options->entry_type ==
        GetRecentlyClosed::Params::Options::ENTRY_TYPE_NONE) {
      // Include both tabs and windows if type is not defined.
      result.push_back(make_linked_ptr(CreateEntryModel(entry).release()));
    } else if (
        (params->options->entry_type ==
            GetRecentlyClosed::Params::Options::ENTRY_TYPE_TAB &&
                entry->type == TabRestoreService::TAB) ||
        (params->options->entry_type ==
            GetRecentlyClosed::Params::Options::ENTRY_TYPE_WINDOW &&
                entry->type == TabRestoreService::WINDOW)) {
      result.push_back(make_linked_ptr(CreateEntryModel(entry).release()));
    }
  }

  results_ = GetRecentlyClosed::Results::Create(result);
  return true;
}

bool SessionRestoreRestoreFunction::RunImpl() {
  scoped_ptr<Restore::Params> params(Restore::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  Browser* browser =
      chrome::FindBrowserWithProfile(profile(),
                                     chrome::HOST_DESKTOP_TYPE_NATIVE);
  if (!browser) {
    error_ = kNoBrowserToRestoreSession;
    return false;
  }

  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  TabRestoreServiceDelegate* delegate =
      TabRestoreServiceDelegate::FindDelegateForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  DCHECK(delegate);
  chrome::HostDesktopType host_desktop_type = browser->host_desktop_type();
  TabRestoreService::Entries entries = tab_restore_service->entries();

  if (entries.empty()) {
    error_ = kRecentlyClosedListEmpty;
    return false;
  }

  if (!params->id) {
    tab_restore_service->RestoreMostRecentEntry(delegate, host_desktop_type);
    return true;
  }

  // Check if the recently closed list contains an entry with the provided id.
  bool is_valid_id = false;
  for (TabRestoreService::Entries::iterator it = entries.begin();
       it != entries.end(); ++it) {
    if ((*it)->id == *params->id) {
      is_valid_id = true;
      break;
    }

    // For Window entries, see if the ID matches a tab. If so, report true for
    // the window as the Entry.
    if ((*it)->type == TabRestoreService::WINDOW) {
      std::vector<TabRestoreService::Tab>& tabs =
          static_cast<TabRestoreService::Window*>(*it)->tabs;
      for (std::vector<TabRestoreService::Tab>::iterator tab_it = tabs.begin();
           tab_it != tabs.end(); ++tab_it) {
        if ((*tab_it).id == *params->id) {
          is_valid_id = true;
          break;
        }
      }
    }
  }

  if (!is_valid_id) {
    error_ = kInvalidSessionId;
    return false;
  }

  tab_restore_service->RestoreEntryById(delegate, *params->id,
      host_desktop_type, UNKNOWN);
  return true;
}

SessionRestoreAPI::SessionRestoreAPI(Profile* profile) {
}

SessionRestoreAPI::~SessionRestoreAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<SessionRestoreAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<SessionRestoreAPI>*
    SessionRestoreAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
