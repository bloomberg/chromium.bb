// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/framebust_block_tab_helper.h"

#include "base/logging.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(FramebustBlockTabHelper);

FramebustBlockTabHelper::~FramebustBlockTabHelper() = default;

// static
void FramebustBlockTabHelper::AddBlockedUrl(const GURL& blocked_url) {
  blocked_urls_.push_back(blocked_url);

  if (observer_)
    observer_->OnBlockedUrlAdded(blocked_url);
}

bool FramebustBlockTabHelper::HasBlockedUrls() const {
  return !blocked_urls_.empty();
}

void FramebustBlockTabHelper::OnBlockedUrlClicked(size_t index) {
  web_contents_->OpenURL(content::OpenURLParams(
      blocked_urls_[index], content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_LINK, false));
  blocked_urls_.clear();
}

void FramebustBlockTabHelper::SetObserver(Observer* observer) {
  DCHECK(!observer_);
  observer_ = observer;
}

void FramebustBlockTabHelper::ClearObserver() {
  DCHECK(observer_);
  observer_ = nullptr;
}

FramebustBlockTabHelper::FramebustBlockTabHelper(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}
