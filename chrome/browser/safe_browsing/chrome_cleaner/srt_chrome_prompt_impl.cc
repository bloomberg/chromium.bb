// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_chrome_prompt_impl.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

using chrome_cleaner::mojom::ChromePrompt;
using chrome_cleaner::mojom::ChromePromptRequest;
using chrome_cleaner::mojom::ElevationStatus;
using chrome_cleaner::mojom::PromptAcceptance;
using chrome_cleaner::mojom::UwSPtr;

ChromePromptImpl::ChromePromptImpl(ChromePromptRequest request,
                                   base::Closure on_connection_closed,
                                   OnPromptUser on_prompt_user)
    : binding_(this, std::move(request)),
      on_prompt_user_(std::move(on_prompt_user)) {
  DCHECK(on_prompt_user_);
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  binding_.set_connection_error_handler(std::move(on_connection_closed));
}

ChromePromptImpl::~ChromePromptImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void ChromePromptImpl::PromptUser(std::vector<UwSPtr> removable_uws_found,
                                  ElevationStatus elevation_status,
                                  ChromePrompt::PromptUserCallback callback) {
  auto files_to_delete = base::MakeUnique<std::set<base::FilePath>>();
  for (const UwSPtr& uws_ptr : removable_uws_found) {
    files_to_delete->insert(uws_ptr->files_to_delete.begin(),
                            uws_ptr->files_to_delete.end());
  }

  if (on_prompt_user_) {
    std::move(on_prompt_user_)
        .Run(std::move(files_to_delete), std::move(callback));
  }
}

}  // namespace safe_browsing
