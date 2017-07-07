// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/preconnect_manager.h"

#include <utility>

#include "content/public/browser/browser_thread.h"

namespace predictors {

PreconnectManager::PreconnectManager(
    base::WeakPtr<Delegate> delegate,
    scoped_refptr<net::URLRequestContextGetter> context_getter)
    : delegate_(std::move(delegate)),
      context_getter_(std::move(context_getter)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PreconnectManager::~PreconnectManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void PreconnectManager::Start(const GURL& url,
                              const std::vector<GURL>& preconnect_origins,
                              const std::vector<GURL>& preresolve_hosts) {
  NOTIMPLEMENTED();
}

void PreconnectManager::Stop(const GURL& url) {
  NOTIMPLEMENTED();
}

}  // namespace predictors
