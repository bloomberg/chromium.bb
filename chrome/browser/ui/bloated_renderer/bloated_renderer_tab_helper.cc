// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bloated_renderer/bloated_renderer_tab_helper.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
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

void BloatedRendererTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(ulan): Use nagivation_handle to ensure that the finished navigation
  // is the same nagivation started by reloading the bloated tab.
  if (reloading_bloated_renderer_) {
    ShowInfoBar(InfoBarService::FromWebContents(web_contents()));
    reloading_bloated_renderer_ = false;
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
  SimpleAlertInfoBarDelegate::Create(
      infobar_service,
      infobars::InfoBarDelegate::BLOATED_RENDERER_INFOBAR_DELEGATE, nullptr,
      l10n_util::GetStringUTF16(IDS_BROWSER_BLOATED_RENDERER_INFOBAR), false);
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

  // TODO(ulan): Check if the navigation controller has POST data.

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
      reloading_bloated_renderer_ = true;
      web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                             check_for_repost);
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
