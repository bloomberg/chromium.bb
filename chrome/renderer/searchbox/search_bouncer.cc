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

SearchBouncer::SearchBouncer() : search_bouncer_binding_(this) {}

SearchBouncer::~SearchBouncer() = default;

// static
SearchBouncer* SearchBouncer::GetInstance() {
  return g_search_bouncer.Pointer();
}

void SearchBouncer::RegisterMojoInterfaces(
    content::AssociatedInterfaceRegistry* associated_interfaces) {
  // Note: Unretained is safe here because this class is a leaky LazyInstance.
  // For the same reason, UnregisterMojoInterfaces isn't required.
  associated_interfaces->AddInterface(base::Bind(
      &SearchBouncer::OnSearchBouncerRequest, base::Unretained(this)));
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

void SearchBouncer::SetSearchURLs(const std::vector<GURL>& search_urls,
                                  const GURL& new_tab_page_url) {
  search_urls_ = search_urls;
  new_tab_page_url_ = new_tab_page_url;
}

void SearchBouncer::OnSearchBouncerRequest(
    chrome::mojom::SearchBouncerAssociatedRequest request) {
  search_bouncer_binding_.Bind(std::move(request));
}
