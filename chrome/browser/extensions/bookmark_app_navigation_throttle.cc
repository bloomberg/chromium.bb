// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_navigation_throttle.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace extensions {

namespace {

enum class ProcessNavigationResult {
  kProceedStartedFromContextMenu,
  kProceedTransitionTyped,
  kProceedTransitionAutoBookmark,
  kProceedTransitionAutoSubframe,
  kProceedTransitionManualSubframe,
  kProceedTransitionGenerated,
  kProceedTransitionAutoToplevel,
  kProceedTransitionReload,
  kProceedTransitionKeyword,
  kProceedTransitionKeywordGenerated,
  kProceedTransitionForwardBack,
  kProceedTransitionFromAddressBar,
  kOpenInChromeProceedOutOfScopeLaunch,
  kProceedInAppSameScope,
  kProceedInBrowserFormSubmission,
  kProceedInBrowserSameScope,
  kCancelPrerenderContents,
  kDeferOpenAppCloseEmptyWebContents,
  kCancelOpenedApp,
  kDeferOpenNewTabInAppOutOfScope,
  // Add ProcessNavigation results immediately above this line. Also
  // update the enum list in tools/metrics/enums.xml accordingly.
  kCount,
};

// Non-app site navigations: The majority of navigations will be in-browser to
// sites for which there is no app installed. These navigations offer no insight
// so we avoid recording their outcome.

void RecordProcessNavigationResult(ProcessNavigationResult result) {
  UMA_HISTOGRAM_ENUMERATION("Extensions.BookmarkApp.NavigationResult", result,
                            ProcessNavigationResult::kCount);
}

void RecordProceedWithTransitionType(ui::PageTransition transition_type) {
  if (PageTransitionCoreTypeIs(transition_type, ui::PAGE_TRANSITION_LINK)) {
    // Link navigations are a special case and shouldn't use this code path.
    NOTREACHED();
  } else if (PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_TYPED)) {
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedTransitionTyped);
  } else if (PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedTransitionAutoBookmark);
  } else if (PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_AUTO_SUBFRAME)) {
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedTransitionAutoSubframe);
  } else if (PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_MANUAL_SUBFRAME)) {
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedTransitionManualSubframe);
  } else if (PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_GENERATED)) {
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedTransitionGenerated);
  } else if (PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_AUTO_TOPLEVEL)) {
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedTransitionAutoToplevel);
  } else if (PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_FORM_SUBMIT)) {
    // Form navigations are a special case and shouldn't use this code path.
    // TODO(crbug.com/772803): Add NOTREACHED() once form navigations are
    // handled.
  } else if (PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_RELOAD)) {
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedTransitionReload);
  } else if (PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_KEYWORD)) {
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedTransitionKeyword);
  } else if (PageTransitionCoreTypeIs(transition_type,
                                      ui::PAGE_TRANSITION_KEYWORD_GENERATED)) {
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedTransitionKeywordGenerated);
  } else {
    NOTREACHED();
  }
}

bool IsWindowedBookmarkApp(const Extension* app,
                           content::BrowserContext* context) {
  if (!app || !app->from_bookmark())
    return false;

  if (GetLaunchContainer(extensions::ExtensionPrefs::Get(context), app) !=
      LAUNCH_CONTAINER_WINDOW) {
    return false;
  }

  return true;
}

scoped_refptr<const Extension> GetAppForURL(
    const GURL& url,
    const content::WebContents* web_contents) {
  content::BrowserContext* context = web_contents->GetBrowserContext();
  for (scoped_refptr<const extensions::Extension> app :
       ExtensionRegistry::Get(context)->enabled_extensions()) {
    if (!IsWindowedBookmarkApp(app.get(), context))
      continue;

    const UrlHandlerInfo* url_handler =
        UrlHandlers::FindMatchingUrlHandler(app.get(), url);
    if (!url_handler)
      continue;

    return app;
  }

  return nullptr;
}

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
BookmarkAppNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DVLOG(1) << "Considering URL for interception: "
           << navigation_handle->GetURL().spec();
  if (!navigation_handle->IsInMainFrame()) {
    DVLOG(1) << "Don't intercept: Navigation is not in main frame.";
    return nullptr;
  }

  content::BrowserContext* browser_context =
      navigation_handle->GetWebContents()->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile->GetProfileType() == Profile::INCOGNITO_PROFILE) {
    DVLOG(1) << "Don't intercept: Navigation is in incognito.";
    return nullptr;
  }

  DVLOG(1) << "Attaching Bookmark App Navigation Throttle.";
  return std::make_unique<extensions::BookmarkAppNavigationThrottle>(
      navigation_handle);
}

BookmarkAppNavigationThrottle::BookmarkAppNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle), weak_ptr_factory_(this) {}

BookmarkAppNavigationThrottle::~BookmarkAppNavigationThrottle() {}

const char* BookmarkAppNavigationThrottle::GetNameForLogging() {
  return "BookmarkAppNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
BookmarkAppNavigationThrottle::WillStartRequest() {
  return ProcessNavigation(false /* is_redirect */);
}

content::NavigationThrottle::ThrottleCheckResult
BookmarkAppNavigationThrottle::WillRedirectRequest() {
  return ProcessNavigation(true /* is_redirect */);
}

content::NavigationThrottle::ThrottleCheckResult
BookmarkAppNavigationThrottle::ProcessNavigation(bool is_redirect) {
  scoped_refptr<const Extension> target_app = GetTargetApp();

  if (navigation_handle()->WasStartedFromContextMenu()) {
    DVLOG(1) << "Don't intercept: Navigation started from the context menu.";

    // See "Non-app site navigations" note above.
    if (target_app) {
      RecordProcessNavigationResult(
          ProcessNavigationResult::kProceedStartedFromContextMenu);
    }

    return content::NavigationThrottle::PROCEED;
  }

  ui::PageTransition transition_type = navigation_handle()->GetPageTransition();

  // When launching an app, if the page redirects to an out-of-scope URL, then
  // continue the navigation in a regular browser window. (Launching an app
  // results in an AUTO_BOOKMARK transition).
  //
  // Note that for non-redirecting app launches, GetAppForWindow() might return
  // null, because the navigation's WebContents might not be attached to a
  // window yet.
  //
  // TODO(crbug.com/789051): Possibly fall through to the logic below to
  // open target in app window, if it belongs to an app.
  if (is_redirect && PageTransitionCoreTypeIs(
                         transition_type, ui::PAGE_TRANSITION_AUTO_BOOKMARK)) {
    auto app_for_window = GetAppForWindow();
    // If GetAppForWindow returned nullptr, we are already in the browser, so
    // don't open a new tab.
    if (app_for_window && app_for_window != GetTargetApp()) {
      DVLOG(1) << "Out-of-scope navigation during launch. Opening in Chrome.";
      RecordProcessNavigationResult(
          ProcessNavigationResult::kOpenInChromeProceedOutOfScopeLaunch);
      Browser* browser = chrome::FindBrowserWithWebContents(
          navigation_handle()->GetWebContents());
      DCHECK(browser);
      chrome::OpenInChrome(browser);
      return content::NavigationThrottle::PROCEED;
    }
  }

  if (!PageTransitionCoreTypeIs(transition_type, ui::PAGE_TRANSITION_LINK) &&
      !PageTransitionCoreTypeIs(transition_type,
                                ui::PAGE_TRANSITION_FORM_SUBMIT)) {
    DVLOG(1) << "Don't intercept: Transition type is "
             << PageTransitionGetCoreTransitionString(transition_type);
    // We are in one of three possible states:
    //   1. In-browser no-target-app navigations,
    //   2. In-browser same-scope navigations, or
    //   3. In-app same-scope navigations
    // Ignore (1) since that's the majority of navigations and offer no insight.
    if (target_app)
      RecordProceedWithTransitionType(transition_type);

    return content::NavigationThrottle::PROCEED;
  }

  int32_t transition_qualifier = PageTransitionGetQualifier(transition_type);
  if (transition_qualifier & ui::PAGE_TRANSITION_FORWARD_BACK) {
    DVLOG(1) << "Don't intercept: Forward or back navigation.";

    // See "Non-app site navigations" note above.
    if (target_app) {
      RecordProcessNavigationResult(
          ProcessNavigationResult::kProceedTransitionForwardBack);
    }
    return content::NavigationThrottle::PROCEED;
  }

  if (transition_qualifier & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) {
    DVLOG(1) << "Don't intercept: Address bar navigation.";

    // See "Non-app site navigations" note above.
    if (target_app) {
      RecordProcessNavigationResult(
          ProcessNavigationResult::kProceedTransitionFromAddressBar);
    }
    return content::NavigationThrottle::PROCEED;
  }

  scoped_refptr<const Extension> app_for_window = GetAppForWindow();

  if (app_for_window == target_app) {
    if (app_for_window) {
      DVLOG(1) << "Don't intercept: The target URL is in the same scope as the "
               << "current app.";

      // We know we are navigating within the same app window (both
      // |app_for_window| and |target_app| are the same and non-null). This is
      // relevant, so record the result.
      RecordProcessNavigationResult(
          ProcessNavigationResult::kProceedInAppSameScope);
    } else {
      DVLOG(1) << "No matching Bookmark App for URL: "
               << navigation_handle()->GetURL();
      // See "Non-app site navigations" note above.
    }
    DVLOG(1) << "Don't intercept: The target URL is in the same scope as the "
             << "current app.";
    return content::NavigationThrottle::PROCEED;
  }

  // If this is a browser tab, and the user is submitting a form, then keep the
  // navigation in the browser tab.
  if (!app_for_window &&
      PageTransitionCoreTypeIs(transition_type,
                               ui::PAGE_TRANSITION_FORM_SUBMIT)) {
    DVLOG(1) << "Keep form submissions in the browser.";
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedInBrowserFormSubmission);
    return content::NavigationThrottle::PROCEED;
  }

  // If this is a browser tab, and the current and target URL are within-scope
  // of the same app, don't intercept the navigation.
  // This ensures that navigating from
  // https://www.youtube.com/ to https://www.youtube.com/some_video doesn't
  // open a new app window if the Youtube app is installed, but navigating from
  // https://www.google.com/ to https://www.google.com/maps does open a new
  // app window if only the Maps app is installed.
  if (!app_for_window && target_app == GetAppForCurrentURL()) {
    DVLOG(1) << "Don't intercept: Keep same-app navigations in the browser.";
    RecordProcessNavigationResult(
        ProcessNavigationResult::kProceedInBrowserSameScope);
    return content::NavigationThrottle::PROCEED;
  }

  if (target_app) {
    auto* prerender_contents = prerender::PrerenderContents::FromWebContents(
        navigation_handle()->GetWebContents());
    if (prerender_contents) {
      // If prerendering, don't launch the app but abort the navigation.
      prerender_contents->Destroy(
          prerender::FINAL_STATUS_NAVIGATION_INTERCEPTED);
      RecordProcessNavigationResult(
          ProcessNavigationResult::kCancelPrerenderContents);
      return content::NavigationThrottle::CANCEL_AND_IGNORE;
    }

    content::NavigationEntry* last_entry = navigation_handle()
                                               ->GetWebContents()
                                               ->GetController()
                                               .GetLastCommittedEntry();
    // We are about to open a new app window context. Record the time since the
    // last navigation in this context. (If it is very small, this context
    // probably redirected immediately, which is a bad user experience.)
    if (last_entry && !last_entry->GetTimestamp().is_null()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Extensions.BookmarkApp.OpenAppDeltaSinceLastNavigation",
          base::Time::Now() - last_entry->GetTimestamp());
    }

    content::NavigationThrottle::ThrottleCheckResult result =
        OpenInAppWindowAndCloseTabIfNecessary(target_app);

    ProcessNavigationResult open_in_app_result;
    switch (result.action()) {
      case content::NavigationThrottle::DEFER:
        open_in_app_result =
            ProcessNavigationResult::kDeferOpenAppCloseEmptyWebContents;
        break;
      case content::NavigationThrottle::CANCEL_AND_IGNORE:
        open_in_app_result = ProcessNavigationResult::kCancelOpenedApp;
        break;
      default:
        NOTREACHED();
        open_in_app_result =
            ProcessNavigationResult::kDeferOpenAppCloseEmptyWebContents;
    }

    RecordProcessNavigationResult(open_in_app_result);
    return result;
  }

  if (app_for_window) {
    // The experience when navigating to an out-of-scope website inside an app
    // window is not great, so we bounce these navigations back to the browser.
    // TODO(crbug.com/774895): Stop bouncing back to the browser once the
    // experience for out-of-scope navigations improves.
    DVLOG(1) << "Open in new tab.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&BookmarkAppNavigationThrottle::OpenInNewTab,
                              weak_ptr_factory_.GetWeakPtr()));
    RecordProcessNavigationResult(
        ProcessNavigationResult::kDeferOpenNewTabInAppOutOfScope);
    return content::NavigationThrottle::DEFER;
  }

  DVLOG(1) << "No matching Bookmark App for URL: "
           << navigation_handle()->GetURL();
  return content::NavigationThrottle::PROCEED;
}

content::NavigationThrottle::ThrottleCheckResult
BookmarkAppNavigationThrottle::OpenInAppWindowAndCloseTabIfNecessary(
    scoped_refptr<const Extension> target_app) {
  content::WebContents* source = navigation_handle()->GetWebContents();
  if (source->GetController().IsInitialNavigation()) {
    // When a new WebContents has no opener, the first navigation will happen
    // synchronously. This could result in us opening the app and then focusing
    // the original WebContents. To avoid this we open the app asynchronously.
    if (!source->HasOpener()) {
      DVLOG(1) << "Deferring opening app.";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&BookmarkAppNavigationThrottle::OpenBookmarkApp,
                                weak_ptr_factory_.GetWeakPtr(), target_app));
    } else {
      OpenBookmarkApp(target_app);
    }

    // According to NavigationThrottle::WillStartRequest's documentation closing
    // a WebContents should be done asynchronously to avoid UAFs. Closing the
    // WebContents will cancel the navigation.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&BookmarkAppNavigationThrottle::CloseWebContents,
                              weak_ptr_factory_.GetWeakPtr()));
    return content::NavigationThrottle::DEFER;
  }

  OpenBookmarkApp(target_app);
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

void BookmarkAppNavigationThrottle::OpenBookmarkApp(
    scoped_refptr<const Extension> bookmark_app) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::WebContents* source = navigation_handle()->GetWebContents();
  content::BrowserContext* browser_context = source->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  AppLaunchParams launch_params(
      profile, bookmark_app.get(), extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::CURRENT_TAB, extensions::SOURCE_URL_HANDLER);
  launch_params.override_url = navigation_handle()->GetURL();

  DVLOG(1) << "Opening app.";
  OpenApplication(launch_params);
}

void BookmarkAppNavigationThrottle::CloseWebContents() {
  DVLOG(1) << "Closing empty tab.";
  navigation_handle()->GetWebContents()->Close();
}

void BookmarkAppNavigationThrottle::OpenInNewTab() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::WebContents* source = navigation_handle()->GetWebContents();
  content::OpenURLParams url_params(navigation_handle()->GetURL(),
                                    navigation_handle()->GetReferrer(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    navigation_handle()->GetPageTransition(),
                                    navigation_handle()->IsRendererInitiated());
  url_params.uses_post = navigation_handle()->IsPost();
  url_params.post_data = navigation_handle()->GetResourceRequestBody();
  url_params.redirect_chain = navigation_handle()->GetRedirectChain();
  url_params.frame_tree_node_id = navigation_handle()->GetFrameTreeNodeId();
  url_params.user_gesture = navigation_handle()->HasUserGesture();
  url_params.started_from_context_menu =
      navigation_handle()->WasStartedFromContextMenu();

  source->OpenURL(url_params);
  CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
}

scoped_refptr<const Extension>
BookmarkAppNavigationThrottle::GetAppForWindow() {
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.BookmarkApp.GetAppForWindowDuration");
  content::WebContents* source = navigation_handle()->GetWebContents();
  content::BrowserContext* context = source->GetBrowserContext();
  Browser* browser = chrome::FindBrowserWithWebContents(source);
  if (!browser || !browser->is_app())
    return nullptr;

  const Extension* app = ExtensionRegistry::Get(context)->GetExtensionById(
      web_app::GetExtensionIdFromApplicationName(browser->app_name()),
      extensions::ExtensionRegistry::ENABLED);

  if (!IsWindowedBookmarkApp(app, context))
    return nullptr;

  // Bookmark Apps for installable websites have scope.
  // TODO(crbug.com/774918): Replace once there is a more explicit indicator
  // of a Bookmark App for an installable website.
  if (UrlHandlers::GetUrlHandlers(app) == nullptr)
    return nullptr;

  return app;
}

scoped_refptr<const Extension> BookmarkAppNavigationThrottle::GetTargetApp() {
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.BookmarkApp.GetTargetAppDuration");
  return GetAppForURL(navigation_handle()->GetURL(),
                      navigation_handle()->GetWebContents());
}

scoped_refptr<const Extension>
BookmarkAppNavigationThrottle::GetAppForCurrentURL() {
  SCOPED_UMA_HISTOGRAM_TIMER(
      "Extensions.BookmarkApp.GetAppForCurrentURLDuration");
  return GetAppForURL(navigation_handle()
                          ->GetWebContents()
                          ->GetMainFrame()
                          ->GetLastCommittedURL(),
                      navigation_handle()->GetWebContents());
}

}  // namespace extensions
