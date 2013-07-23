// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/core_tab_helper.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_states.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(CoreTabHelper);

CoreTabHelper::CoreTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      delegate_(NULL),
      content_restrictions_(0) {
}

CoreTabHelper::~CoreTabHelper() {
}

string16 CoreTabHelper::GetDefaultTitle() {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
}

string16 CoreTabHelper::GetStatusText() const {
  if (!web_contents()->IsLoading() ||
      web_contents()->GetLoadState().state == net::LOAD_STATE_IDLE) {
    return string16();
  }

  switch (web_contents()->GetLoadState().state) {
    case net::LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL:
    case net::LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_SOCKET_SLOT);
    case net::LOAD_STATE_WAITING_FOR_DELEGATE:
      if (!web_contents()->GetLoadState().param.empty()) {
        return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_DELEGATE,
                                          web_contents()->GetLoadState().param);
      } else {
        return l10n_util::GetStringUTF16(
            IDS_LOAD_STATE_WAITING_FOR_DELEGATE_GENERIC);
      }
    case net::LOAD_STATE_WAITING_FOR_CACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_CACHE);
    case net::LOAD_STATE_WAITING_FOR_APPCACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_APPCACHE);
    case net::LOAD_STATE_ESTABLISHING_PROXY_TUNNEL:
      return
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_ESTABLISHING_PROXY_TUNNEL);
    case net::LOAD_STATE_DOWNLOADING_PROXY_SCRIPT:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_DOWNLOADING_PROXY_SCRIPT);
    case net::LOAD_STATE_RESOLVING_PROXY_FOR_URL:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_PROXY_FOR_URL);
    case net::LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT:
      return l10n_util::GetStringUTF16(
          IDS_LOAD_STATE_RESOLVING_HOST_IN_PROXY_SCRIPT);
    case net::LOAD_STATE_RESOLVING_HOST:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_HOST);
    case net::LOAD_STATE_CONNECTING:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_CONNECTING);
    case net::LOAD_STATE_SSL_HANDSHAKE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SSL_HANDSHAKE);
    case net::LOAD_STATE_SENDING_REQUEST:
      if (web_contents()->GetUploadSize()) {
        return l10n_util::GetStringFUTF16Int(
            IDS_LOAD_STATE_SENDING_REQUEST_WITH_PROGRESS,
            static_cast<int>((100 * web_contents()->GetUploadPosition()) /
                web_contents()->GetUploadSize()));
      } else {
        return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SENDING_REQUEST);
      }
    case net::LOAD_STATE_WAITING_FOR_RESPONSE:
      return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_RESPONSE,
                                        web_contents()->GetLoadStateHost());
    // Ignore net::LOAD_STATE_READING_RESPONSE and net::LOAD_STATE_IDLE
    case net::LOAD_STATE_IDLE:
    case net::LOAD_STATE_READING_RESPONSE:
      break;
  }

  return string16();
}

void CoreTabHelper::OnCloseStarted() {
  if (close_start_time_.is_null())
    close_start_time_ = base::TimeTicks::Now();
}

void CoreTabHelper::OnCloseCanceled() {
  close_start_time_ = base::TimeTicks();
  before_unload_end_time_ = base::TimeTicks();
  unload_detached_start_time_ = base::TimeTicks();
}

void CoreTabHelper::OnUnloadStarted() {
  before_unload_end_time_ = base::TimeTicks::Now();
}

void CoreTabHelper::OnUnloadDetachedStarted() {
  if (unload_detached_start_time_.is_null())
    unload_detached_start_time_ = base::TimeTicks::Now();
}

void CoreTabHelper::UpdateContentRestrictions(int content_restrictions) {
  content_restrictions_ = content_restrictions;
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;

  browser->command_controller()->ContentRestrictionsChanged();
#endif
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

void CoreTabHelper::DidStartLoading(content::RenderViewHost* render_view_host) {
  UpdateContentRestrictions(0);
}

void CoreTabHelper::WasShown() {
  WebCacheManager::GetInstance()->ObserveActivity(
      web_contents()->GetRenderProcessHost()->GetID());
}

void CoreTabHelper::WebContentsDestroyed(WebContents* web_contents) {
  // OnCloseStarted isn't called in unit tests.
  if (!close_start_time_.is_null()) {
    bool fast_tab_close_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableFastUnload);

    if (fast_tab_close_enabled) {
      base::TimeTicks now = base::TimeTicks::Now();
      base::TimeDelta close_time = now - close_start_time_;
      UMA_HISTOGRAM_TIMES("Tab.Close", close_time);

      base::TimeTicks unload_start_time = close_start_time_;
      base::TimeTicks unload_end_time = now;
      if (!before_unload_end_time_.is_null())
        unload_start_time = before_unload_end_time_;
      if (!unload_detached_start_time_.is_null())
        unload_end_time = unload_detached_start_time_;
      base::TimeDelta unload_time = unload_end_time - unload_start_time;
      UMA_HISTOGRAM_TIMES("Tab.Close.UnloadTime", unload_time);
    } else {
      base::TimeTicks now = base::TimeTicks::Now();
      base::TimeTicks unload_start_time = close_start_time_;
      if (!before_unload_end_time_.is_null())
        unload_start_time = before_unload_end_time_;
      UMA_HISTOGRAM_TIMES("Tab.Close", now - close_start_time_);
      UMA_HISTOGRAM_TIMES("Tab.Close.UnloadTime", now - unload_start_time);
    }
  }

}

void CoreTabHelper::BeforeUnloadFired(const base::TimeTicks& proceed_time) {
  before_unload_end_time_ = proceed_time;
}

void CoreTabHelper::BeforeUnloadDialogCancelled() {
  OnCloseCanceled();
}
