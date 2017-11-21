// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"

#include <memory>
#include <utility>

#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

FramebustBlockMessageDelegate::FramebustBlockMessageDelegate(
    content::WebContents* web_contents,
    const GURL& blocked_url,
    base::OnceClosure click_closure)
    : click_closure_(std::move(click_closure)),
      web_contents_(web_contents),
      blocked_url_(blocked_url) {}

FramebustBlockMessageDelegate::~FramebustBlockMessageDelegate() = default;

const GURL& FramebustBlockMessageDelegate::GetBlockedUrl() const {
  return blocked_url_;
}

void FramebustBlockMessageDelegate::AcceptIntervention() {}

void FramebustBlockMessageDelegate::DeclineIntervention() {
  if (!click_closure_.is_null())
    std::move(click_closure_).Run();
  web_contents_->OpenURL(content::OpenURLParams(
      blocked_url_, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}
