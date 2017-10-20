// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#endif

FramebustBlockMessageDelegate::FramebustBlockMessageDelegate(
    content::WebContents* web_contents,
    const GURL& blocked_url,
    base::OnceClosure click_closure)
    : click_closure_(std::move(click_closure)),
      web_contents_(web_contents),
      blocked_url_(blocked_url) {}

FramebustBlockMessageDelegate::~FramebustBlockMessageDelegate() = default;

int FramebustBlockMessageDelegate::GetIconId() const {
#if defined(OS_ANDROID)
  return IDR_ANDROID_INFOBAR_FRAMEBUST;
#else
  NOTREACHED();
  return 0;
#endif
}

base::string16 FramebustBlockMessageDelegate::GetLongMessage() const {
#if defined(OS_ANDROID)
  return l10n_util::GetStringUTF16(IDS_REDIRECT_BLOCKED_MESSAGE);
#else
  NOTREACHED();
  return base::string16();
#endif
}

base::string16 FramebustBlockMessageDelegate::GetShortMessage() const {
#if defined(OS_ANDROID)
  return l10n_util::GetStringUTF16(IDS_REDIRECT_BLOCKED_SHORT_MESSAGE);
#else
  NOTREACHED();
  return base::string16();
#endif
}

const GURL& FramebustBlockMessageDelegate::GetBlockedUrl() const {
  return blocked_url_;
}

void FramebustBlockMessageDelegate::OnLinkClicked() {
  if (!click_closure_.is_null())
    std::move(click_closure_).Run();
  web_contents_->OpenURL(content::OpenURLParams(
      blocked_url_, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}
