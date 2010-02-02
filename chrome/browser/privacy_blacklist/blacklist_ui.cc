// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_ui.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/privacy_blacklist/blacklist_request_info.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"

// static
void BlacklistUI::OnNonvisualContentBlocked(const URLRequest* /*request*/) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // TODO

  /* This code will probably be useful when this function is implemented.
  BlacklistRequestInfo* request_info =
      BlacklistRequestInfo::FromURLRequest(request);
  const Blacklist* blacklist = request_info->GetBlacklist();
  scoped_ptr<Blacklist::Match> match(blacklist->FindMatch(request->url()));
  const ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  */
}
