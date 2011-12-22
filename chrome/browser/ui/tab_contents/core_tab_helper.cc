// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/core_tab_helper.h"

#include "chrome/browser/renderer_host/web_cache_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

CoreTabHelper::CoreTabHelper(TabContentsWrapper* wrapper)
    : TabContentsObserver(wrapper->tab_contents()),
      delegate_(NULL),
      wrapper_(wrapper) {
}

CoreTabHelper::~CoreTabHelper() {
}

string16 CoreTabHelper::GetDefaultTitle() {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
}

string16 CoreTabHelper::GetStatusText() const {
  if (!tab_contents()->IsLoading() ||
      tab_contents()->GetLoadState().state == net::LOAD_STATE_IDLE) {
    return string16();
  }

  switch (tab_contents()->GetLoadState().state) {
    case net::LOAD_STATE_WAITING_FOR_DELEGATE:
      return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_DELEGATE,
                                        tab_contents()->GetLoadState().param);
    case net::LOAD_STATE_WAITING_FOR_CACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_CACHE);
    case net::LOAD_STATE_WAITING_FOR_APPCACHE:
      return l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_APPCACHE);
    case net::LOAD_STATE_ESTABLISHING_PROXY_TUNNEL:
      return
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_ESTABLISHING_PROXY_TUNNEL);
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
      if (tab_contents()->GetUploadSize()) {
        return l10n_util::GetStringFUTF16Int(
            IDS_LOAD_STATE_SENDING_REQUEST_WITH_PROGRESS,
            static_cast<int>((100 * tab_contents()->GetUploadPosition()) /
                tab_contents()->GetUploadSize()));
      } else {
        return l10n_util::GetStringUTF16(IDS_LOAD_STATE_SENDING_REQUEST);
      }
    case net::LOAD_STATE_WAITING_FOR_RESPONSE:
      return l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_RESPONSE,
                                        tab_contents()->GetLoadStateHost());
    // Ignore net::LOAD_STATE_READING_RESPONSE and net::LOAD_STATE_IDLE
    case net::LOAD_STATE_IDLE:
    case net::LOAD_STATE_READING_RESPONSE:
      break;
  }

  return string16();
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsObserver overrides

void CoreTabHelper::DidBecomeSelected() {
  WebCacheManager::GetInstance()->ObserveActivity(
      tab_contents()->GetRenderProcessHost()->GetID());
}
