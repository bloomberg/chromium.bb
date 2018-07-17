// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bloated_renderer/bloated_renderer_tab_helper.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/page_importance_signals.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(BloatedRendererTabHelper);

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BloatedRendererHandlingInBrowser {
  kReloaded = 0,
  kCannotReload = 1,
  kCannotShutdown = 2,
  kMaxValue = kCannotShutdown
};

void RecordBloatedRendererHandling(BloatedRendererHandlingInBrowser handling) {
  UMA_HISTOGRAM_ENUMERATION("BloatedRenderer.HandlingInBrowser", handling);
}
}  // anonymous namespace

BloatedRendererTabHelper::BloatedRendererTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {
  auto* page_signal_receiver =
      resource_coordinator::PageSignalReceiver::GetInstance();
  if (page_signal_receiver) {
    // PageSignalReceiver is not available if the resource coordinator is not
    // enabled.
    page_signal_receiver->AddObserver(this);
  }
}

void BloatedRendererTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (state_ == State::kRequestingReload) {
    saved_navigation_id_ = navigation_handle->GetNavigationId();
    state_ = State::kStartedNavigation;
  }
}

void BloatedRendererTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (state_ == State::kStartedNavigation &&
      saved_navigation_id_ == navigation_handle->GetNavigationId()) {
    ShowInfoBar(InfoBarService::FromWebContents(web_contents()));
    state_ = State::kInactive;
    saved_navigation_id_ = 0;
  }
}

void BloatedRendererTabHelper::WebContentsDestroyed() {
  auto* page_signal_receiver =
      resource_coordinator::PageSignalReceiver::GetInstance();
  if (page_signal_receiver) {
    // PageSignalReceiver is not available if the resource coordinator is not
    // enabled.
    page_signal_receiver->RemoveObserver(this);
  }
}

void BloatedRendererTabHelper::ShowInfoBar(InfoBarService* infobar_service) {
  if (!infobar_service) {
    // No infobar service in unit-tests.
    return;
  }
  const bool auto_expire_on_navigation = true;
  SimpleAlertInfoBarDelegate::Create(
      infobar_service,
      infobars::InfoBarDelegate::BLOATED_RENDERER_INFOBAR_DELEGATE, nullptr,
      l10n_util::GetStringUTF16(IDS_BROWSER_BLOATED_RENDERER_INFOBAR),
      auto_expire_on_navigation);
}

bool BloatedRendererTabHelper::CanReloadBloatedTab() {
  if (web_contents()->IsCrashed())
    return false;

  // Do not reload tabs that don't have a valid URL (most probably they have
  // just been opened and discarding them would lose the URL).
  if (!web_contents()->GetLastCommittedURL().is_valid() ||
      web_contents()->GetLastCommittedURL().is_empty()) {
    return false;
  }

  // Do not reload tabs in which the user has entered text in a form.
  if (web_contents()->GetPageImportanceSignals().had_form_interaction)
    return false;

  // Do not reload if no entry was committed.
  content::NavigationEntry* committed_entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!committed_entry)
    return false;

  // Do not reload if the visible entry does not match the last committed entry.
  // This means that the entry is either transient or pending.
  content::NavigationEntry* visible_entry =
      web_contents()->GetController().GetVisibleEntry();
  if (visible_entry != committed_entry)
    return false;

  // Do not reload if the last committed entry has post data.
  if (committed_entry->GetHasPostData())
    return false;

  return true;
}

void BloatedRendererTabHelper::OnRendererIsBloated(
    content::WebContents* bloated_web_contents,
    const resource_coordinator::PageNavigationIdentity& page_navigation_id) {
  if (web_contents() != bloated_web_contents) {
    // Ignore if the notification is about a different tab.
    return;
  }
  auto* page_signal_receiver =
      resource_coordinator::PageSignalReceiver::GetInstance();
  DCHECK_NE(nullptr, page_signal_receiver);
  if (page_navigation_id.navigation_id !=
      page_signal_receiver->GetNavigationIDForWebContents(web_contents())) {
    // Ignore if the notification is pursuant to an earlier navigation.
    return;
  }

  if (CanReloadBloatedTab()) {
    const size_t expected_page_count = 1u;
    const bool skip_unload_handlers = true;
    content::RenderProcessHost* renderer =
        web_contents()->GetMainFrame()->GetProcess();
    if (renderer->FastShutdownIfPossible(expected_page_count,
                                         skip_unload_handlers)) {
      const bool check_for_repost = true;
      // Clear the state and the saved navigation id.
      state_ = State::kRequestingReload;
      saved_navigation_id_ = 0;
      web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                             check_for_repost);
      DCHECK_EQ(State::kStartedNavigation, state_);
      RecordBloatedRendererHandling(
          BloatedRendererHandlingInBrowser::kReloaded);
    } else {
      RecordBloatedRendererHandling(
          BloatedRendererHandlingInBrowser::kCannotShutdown);
    }
  } else {
    RecordBloatedRendererHandling(
        BloatedRendererHandlingInBrowser::kCannotReload);
  }
}
