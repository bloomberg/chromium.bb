// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/search_bouncer.h"

#include "base/lazy_instance.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/search/search_urls.h"
#include "content/public/renderer/render_thread_observer.h"
#include "ipc/ipc_message_macros.h"

namespace {
base::LazyInstance<SearchBouncer>::Leaky g_search_bouncer =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

SearchBouncer::SearchBouncer() {
}

SearchBouncer::~SearchBouncer() {
}

// static
SearchBouncer* SearchBouncer::GetInstance() {
  return g_search_bouncer.Pointer();
}

bool SearchBouncer::ShouldFork(const GURL& url) const {
  if (!url.is_valid())
    return false;
  for (std::vector<GURL>::const_iterator it = search_urls_.begin();
       it != search_urls_.end(); ++it) {
    if (search::MatchesOriginAndPath(url, *it)) {
      return true;
    }
  }
  return IsNewTabPage(url);
}

bool SearchBouncer::IsNewTabPage(const GURL& url) const {
  return url.is_valid() && url == new_tab_page_url_;
}

bool SearchBouncer::OnControlMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SearchBouncer, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetSearchURLs, OnSetSearchURLs)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void SearchBouncer::OnSetSearchURLs(
    std::vector<GURL> search_urls,
    GURL new_tab_page_url) {
  search_urls_ = search_urls;
  new_tab_page_url_ = new_tab_page_url;
}
