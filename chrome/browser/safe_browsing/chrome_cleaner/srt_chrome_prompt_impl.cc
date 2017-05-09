// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_chrome_prompt_impl.h"

#include "base/logging.h"

namespace safe_browsing {

using chrome_cleaner::mojom::ChromePrompt;
using chrome_cleaner::mojom::ChromePromptRequest;
using chrome_cleaner::mojom::ElevationStatus;
using chrome_cleaner::mojom::PromptAcceptance;
using chrome_cleaner::mojom::UwSPtr;

ChromePromptImpl::ChromePromptImpl(ChromePromptRequest request,
                                   base::Closure on_connection_closed)
    : binding_(this, std::move(request)) {
  binding_.set_connection_error_handler(std::move(on_connection_closed));
}

ChromePromptImpl::~ChromePromptImpl() {}

void ChromePromptImpl::PromptUser(std::vector<UwSPtr> removable_uws_found,
                                  ElevationStatus elevation_status,
                                  ChromePrompt::PromptUserCallback callback) {
  // Placeholder. The actual implementation will show the prompt dialog to the
  // user and invoke this callback depending on the user's response.
  std::move(callback).Run(PromptAcceptance::DENIED);
}

}  // namespace safe_browsing
