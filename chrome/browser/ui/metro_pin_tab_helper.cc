// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/metro_pin_tab_helper.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_WIN)
#include "base/win/metro.h"
#endif

int MetroPinTabHelper::kUserDataKey;

MetroPinTabHelper::MetroPinTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      is_pinned_(false) {}

MetroPinTabHelper::~MetroPinTabHelper() {}

void MetroPinTabHelper::TogglePinnedToStartScreen() {
#if defined(OS_WIN)
  HMODULE metro_module = base::win::GetMetroModule();
  if (metro_module) {
    typedef void (*MetroTogglePinnedToStartScreen)(const string16&,
        const string16&);
    MetroTogglePinnedToStartScreen metro_toggle_pinned_to_start_screen =
        reinterpret_cast<MetroTogglePinnedToStartScreen>(
            ::GetProcAddress(metro_module, "MetroTogglePinnedToStartScreen"));
    if (!metro_toggle_pinned_to_start_screen) {
      NOTREACHED();
      return;
    }

    GURL url = web_contents()->GetURL();
    string16 title = web_contents()->GetTitle();
    VLOG(1) << __FUNCTION__ << " calling pin with title: " << title
            << " and url " << UTF8ToUTF16(url.spec());
    metro_toggle_pinned_to_start_screen(title, UTF8ToUTF16(url.spec()));
    // TODO(benwells): This will update the state incorrectly if the user
    // cancels. To fix this some sort of callback needs to be introduced as
    // the pinning happens on another thread.
    is_pinned_ = !is_pinned_;
    return;
  }
#endif
}

void MetroPinTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& /*details*/,
    const content::FrameNavigateParams& /*params*/) {
  UpdatePinnedStateForCurrentURL();
}

void MetroPinTabHelper::UpdatePinnedStateForCurrentURL() {
#if defined(OS_WIN)
  HMODULE metro_module = base::win::GetMetroModule();
  if (metro_module) {
    typedef BOOL (*MetroIsPinnedToStartScreen)(const string16&);
    MetroIsPinnedToStartScreen metro_is_pinned_to_start_screen =
        reinterpret_cast<MetroIsPinnedToStartScreen>(
            ::GetProcAddress(metro_module, "MetroIsPinnedToStartScreen"));
    if (!metro_is_pinned_to_start_screen) {
      NOTREACHED();
      return;
    }

    GURL url = web_contents()->GetURL();
    is_pinned_ = metro_is_pinned_to_start_screen(UTF8ToUTF16(url.spec())) != 0;
    VLOG(1) << __FUNCTION__ << " with url " << UTF8ToUTF16(url.spec())
            << " result: " << is_pinned_;
  }
#endif
}
