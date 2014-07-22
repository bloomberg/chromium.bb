// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_service.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "extensions/common/constants.h"
#endif

namespace {
// The singleton instance of TranslateService.
TranslateService* g_translate_service = NULL;
}

TranslateService::TranslateService() {
  resource_request_allowed_notifier_.Init(this);
}

TranslateService::~TranslateService() {}

// static
void TranslateService::Initialize() {
  if (g_translate_service)
    return;

  g_translate_service = new TranslateService;
  // Initialize the allowed state for resource requests.
  g_translate_service->OnResourceRequestsAllowed();
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->set_request_context(
      g_browser_process->system_request_context());
  download_manager->set_application_locale(
      g_browser_process->GetApplicationLocale());
}

// static
void TranslateService::Shutdown(bool cleanup_pending_fetcher) {
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  if (cleanup_pending_fetcher) {
    download_manager->Shutdown();
  } else {
    // This path is only used by browser tests.
    download_manager->set_request_context(NULL);
  }
}

// static
void TranslateService::InitializeForTesting() {
  if (!g_translate_service) {
    TranslateService::Initialize();
  } else {
    translate::TranslateDownloadManager::GetInstance()->ResetForTesting();
    g_translate_service->OnResourceRequestsAllowed();
  }
}

// static
void TranslateService::ShutdownForTesting() {
  translate::TranslateDownloadManager::GetInstance()->Shutdown();
}

void TranslateService::OnResourceRequestsAllowed() {
  translate::TranslateLanguageList* language_list =
      translate::TranslateDownloadManager::GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED();
    return;
  }

  language_list->SetResourceRequestsAllowed(
      resource_request_allowed_notifier_.ResourceRequestsAllowed());
}

// static
bool TranslateService::IsTranslateBubbleEnabled() {
#if defined(USE_AURA)
  return true;
#elif defined(OS_MACOSX)
  // The bubble UX is experimental on Mac OS X.
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTranslateNewUX);
#else
  // The bubble UX is not implemented on other platforms.
  return false;
#endif
}

// static
std::string TranslateService::GetTargetLanguage(PrefService* prefs) {
  std::vector<std::string> accept_languages_list;
  base::SplitString(prefs->GetString(prefs::kAcceptLanguages), ',',
                    &accept_languages_list);
  return translate::TranslateManager::GetTargetLanguage(accept_languages_list);
}

// static
bool TranslateService::IsTranslatableURL(const GURL& url) {
  // A URLs is translatable unless it is one of the following:
  // - empty (can happen for popups created with window.open(""))
  // - an internal URL (chrome:// and others)
  // - the devtools (which is considered UI)
  // - Chrome OS file manager extension
  // - an FTP page (as FTP pages tend to have long lists of filenames that may
  //   confuse the CLD)
  return !url.is_empty() && !url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(content::kChromeDevToolsScheme) &&
#if defined(OS_CHROMEOS)
         !(url.SchemeIs(extensions::kExtensionScheme) &&
           url.DomainIs(file_manager::kFileManagerAppId)) &&
#endif
         !url.SchemeIs(url::kFtpScheme);
}
