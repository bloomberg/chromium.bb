// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/common/content_features.h"
#include "extensions/features/features.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/md_history_ui.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#endif

bool FixupBrowserAboutURL(GURL* url,
                          content::BrowserContext* browser_context) {
  // Ensure that any cleanup done by FixupURL happens before the rewriting
  // phase that determines the virtual URL, by including it in an initial
  // URLHandler.  This prevents minor changes from producing a virtual URL,
  // which could lead to a URL spoof.
  *url = url_formatter::FixupURL(url->possibly_invalid_spec(), std::string());
  return true;
}

bool WillHandleBrowserAboutURL(GURL* url,
                               content::BrowserContext* browser_context) {
  // TODO(msw): Eliminate "about:*" constants and literals from code and tests,
  //            then hopefully we can remove this forced fixup.
  FixupBrowserAboutURL(url, browser_context);

  // Check that about: URLs are fixed up to chrome: by url_formatter::FixupURL.
  DCHECK((*url == url::kAboutBlankURL) ||
         !url->SchemeIs(url::kAboutScheme));

  // Only handle chrome://foo/, url_formatter::FixupURL translates about:foo.
  if (!url->SchemeIs(content::kChromeUIScheme))
    return false;

  std::string host(url->host());
  std::string path;
  // Replace about with chrome-urls.
  if (host == chrome::kChromeUIAboutHost)
    host = chrome::kChromeUIChromeURLsHost;
  // Replace cache with view-http-cache.
  if (host == chrome::kChromeUICacheHost) {
    host = content::kChromeUINetworkViewCacheHost;
  // Replace sync with sync-internals (for legacy reasons).
  } else if (host == chrome::kChromeUISyncHost) {
    host = chrome::kChromeUISyncInternalsHost;
  // Redirect chrome://extensions.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  } else if (host == chrome::kChromeUIExtensionsHost) {
    // If the material design extensions page is enabled, it gets its own host.
    // Otherwise, it's handled by the uber settings page.
    if (base::FeatureList::IsEnabled(features::kMaterialDesignExtensions)) {
      host = chrome::kChromeUIExtensionsHost;
      path = url->path();
    } else {
      host = chrome::kChromeUIUberHost;
      path = chrome::kChromeUIExtensionsHost + url->path();
    }
  // Redirect chrome://settings/extensions (legacy URL).
  } else if (host == chrome::kChromeUISettingsHost &&
             url->path() ==
                 std::string("/") + chrome::kDeprecatedExtensionsSubPage) {
    host = chrome::kChromeUIUberHost;
    path = chrome::kChromeUIExtensionsHost;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  // Redirect chrome://history.
  } else if (host == chrome::kChromeUIHistoryHost) {
#if defined(OS_ANDROID)
    // TODO(twellington): remove this after native Android history launches.
    // See http://crbug.com/654071.
    if (!base::FeatureList::IsEnabled(features::kNativeAndroidHistoryManager)) {
      // On Android, redirect directly to chrome://history-frame since
      // uber page is unsupported.
      host = chrome::kChromeUIHistoryFrameHost;
    }
#else
    // Material design history is handled on the top-level chrome://history
    // host.
    if (base::FeatureList::IsEnabled(features::kMaterialDesignHistory)) {
      host = chrome::kChromeUIHistoryHost;
      path = url->path();
    } else {
      host = chrome::kChromeUIUberHost;
      path = chrome::kChromeUIHistoryHost + url->path();
    }
#endif
  // Redirect chrome://settings, unless MD settings is enabled.
  } else if (host == chrome::kChromeUISettingsHost) {
    if (base::FeatureList::IsEnabled(features::kMaterialDesignSettings)) {
      return true;  // Prevent further rewriting - this is a valid URL.
    } else if (::switches::SettingsWindowEnabled()) {
      host = chrome::kChromeUISettingsFrameHost;
    } else {
      host = chrome::kChromeUIUberHost;
      path = chrome::kChromeUISettingsHost + url->path();
    }
  // Redirect chrome://help, unless MD settings is enabled.
  } else if (host == chrome::kChromeUIHelpHost) {
    if (base::FeatureList::IsEnabled(features::kMaterialDesignSettings)) {
      return false;  // Handled in the HandleWebUI handler.
    } else if (::switches::SettingsWindowEnabled()) {
      host = chrome::kChromeUISettingsFrameHost;
      if (url->path().empty() || url->path() == "/")
        path = chrome::kChromeUIHelpHost;
    } else {
      host = chrome::kChromeUIUberHost;
      path = chrome::kChromeUIHelpHost + url->path();
    }
  }

  GURL::Replacements replacements;
  replacements.SetHostStr(host);
  if (!path.empty())
    replacements.SetPathStr(path);
  *url = url->ReplaceComponents(replacements);

  // Having re-written the URL, make the chrome: handler process it.
  return false;
}

bool HandleNonNavigationAboutURL(const GURL& url) {
  const std::string spec(url.spec());

  if (base::LowerCaseEqualsASCII(spec, chrome::kChromeUIRestartURL)) {
    // Call AttemptRestart after chrome::Navigate() completes to avoid access of
    // gtk objects after they are destroyed by BrowserWindowGtk::Close().
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&chrome::AttemptRestart));
    return true;
  } else if (base::LowerCaseEqualsASCII(spec, chrome::kChromeUIQuitURL)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&chrome::AttemptExit));
    return true;
  }

  return false;
}
