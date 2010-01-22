// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_prefs.h"

#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/extensions/extension_dom_ui.h"
#include "chrome/browser/extensions/extensions_ui.h"
#include "chrome/browser/external_protocol_handler.h"
#include "chrome/browser/form_field_history_manager.h"
#include "chrome/browser/google_url_tracker.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/page_info_model.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/keyword_editor_controller.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/task_manager.h"

#if defined(TOOLKIT_VIEWS)  // TODO(port): whittle this down as we port
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/frame/browser_view.h"
#endif

#if defined(TOOLKIT_GTK)
#include "chrome/browser/gtk/browser_window_gtk.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/preferences.h"
#endif

namespace browser {

void RegisterAllPrefs(PrefService* user_prefs, PrefService* local_state) {
  RegisterLocalState(local_state);
  RegisterUserPrefs(user_prefs);
}

void RegisterLocalState(PrefService* local_state) {
  // Prefs in Local State
  Browser::RegisterPrefs(local_state);
  WebCacheManager::RegisterPrefs(local_state);
  ExternalProtocolHandler::RegisterPrefs(local_state);
  GoogleURLTracker::RegisterPrefs(local_state);
  IntranetRedirectDetector::RegisterPrefs(local_state);
  KeywordEditorController::RegisterPrefs(local_state);
  MetricsLog::RegisterPrefs(local_state);
  MetricsService::RegisterPrefs(local_state);
  SafeBrowsingService::RegisterPrefs(local_state);
  browser_shutdown::RegisterPrefs(local_state);
  chrome_browser_net::RegisterPrefs(local_state);
  bookmark_utils::RegisterPrefs(local_state);
  PageInfoModel::RegisterPrefs(local_state);
#if defined(TOOLKIT_VIEWS)  // TODO(port): whittle this down as we port
  BrowserView::RegisterBrowserViewPrefs(local_state);
#endif
  TaskManager::RegisterPrefs(local_state);
}

void RegisterUserPrefs(PrefService* user_prefs) {
  // User prefs
  AutoFillManager::RegisterUserPrefs(user_prefs);
  SessionStartupPref::RegisterUserPrefs(user_prefs);
  Browser::RegisterUserPrefs(user_prefs);
  PasswordManager::RegisterUserPrefs(user_prefs);
  chrome_browser_net::RegisterUserPrefs(user_prefs);
  DownloadManager::RegisterUserPrefs(user_prefs);
  SSLManager::RegisterUserPrefs(user_prefs);
  bookmark_utils::RegisterUserPrefs(user_prefs);
  FormFieldHistoryManager::RegisterUserPrefs(user_prefs);
  TabContents::RegisterUserPrefs(user_prefs);
  TemplateURLPrepopulateData::RegisterUserPrefs(user_prefs);
  ExtensionDOMUI::RegisterUserPrefs(user_prefs);
  ExtensionsUI::RegisterUserPrefs(user_prefs);
  NewTabUI::RegisterUserPrefs(user_prefs);
  BlockedPopupContainer::RegisterUserPrefs(user_prefs);
  HostZoomMap::RegisterUserPrefs(user_prefs);
  DevToolsManager::RegisterUserPrefs(user_prefs);
#if defined(TOOLKIT_VIEWS)  // TODO(port): whittle this down as we port.
  BrowserActionsContainer::RegisterUserPrefs(user_prefs);
#endif
#if defined(TOOLKIT_GTK)
  BrowserWindowGtk::RegisterUserPrefs(user_prefs);
#endif
#if defined(OS_CHROMEOS)
  chromeos::Preferences::RegisterUserPrefs(user_prefs);
#endif
}

}  // namespace browser
