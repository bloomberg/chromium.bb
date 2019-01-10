// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_service_delegate.h"

#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/taskbar_decorator_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#include "chrome/common/mac/app_shim.mojom.h"
#endif

namespace {

#if defined(OS_MACOSX)
void SetAppShimBadgeLabel(content::WebContents* contents,
                          const std::string& badge_label) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  auto* shim_handler = apps::ExtensionAppShimHandler::Get();
  if (!shim_handler)
    return;

  AppShimHost* shim_host = shim_handler->GetHostForBrowser(browser);
  if (!shim_host)
    return;

  chrome::mojom::AppShim* shim = shim_host->GetAppShim();
  if (!shim)
    return;

  shim->SetBadgeLabel(badge_label);
}
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
std::string GetBadgeString(base::Optional<uint64_t> badge_content) {
  if (!badge_content)
    return "•";

  if (badge_content > 99u) {
    return base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
        IDS_SATURATED_BADGE_CONTENT, base::FormatNumber(99)));
  }

  return base::UTF16ToUTF8(base::FormatNumber(badge_content.value()));
}
#endif

void SetBadgeImpl(content::WebContents* contents,
                  base::Optional<uint64_t> badge_content) {
#if defined(OS_WIN)
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  auto* window = browser->window()->GetNativeWindow();
  chrome::DrawTaskbarDecorationString(window, GetBadgeString(badge_content));
#elif defined(OS_MACOSX)
  SetAppShimBadgeLabel(contents, GetBadgeString(badge_content));
#endif
}

void ClearBadgeImpl(content::WebContents* contents) {
#if defined(OS_WIN)
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  // Restore the decoration to whatever it is naturally (either nothing or a
  // profile picture badge).
  browser_view->frame()->GetFrameView()->UpdateTaskbarDecoration();
#elif defined(OS_MACOSX)
  SetAppShimBadgeLabel(contents, "");
#endif
}

}  // namespace

BadgeServiceDelegate::BadgeServiceDelegate()
    : on_set_badge_(base::BindRepeating(&SetBadgeImpl)),
      on_clear_badge_(base::BindRepeating(&ClearBadgeImpl)) {}

BadgeServiceDelegate::~BadgeServiceDelegate() {}

void BadgeServiceDelegate::SetBadge(content::WebContents* contents,
                                    base::Optional<uint64_t> badge_content) {
  on_set_badge_.Run(contents, badge_content);
}

void BadgeServiceDelegate::ClearBadge(content::WebContents* contents) {
  on_clear_badge_.Run(contents);
}

void BadgeServiceDelegate::SetImplForTesting(
    SetBadgeCallback on_set_badge,
    ClearBadgeCallback on_clear_badge) {
  on_set_badge_ = on_set_badge;
  on_clear_badge_ = on_clear_badge;
}
