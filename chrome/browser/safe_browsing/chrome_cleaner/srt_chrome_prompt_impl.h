// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_CHROME_PROMPT_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_CHROME_PROMPT_IMPL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace safe_browsing {

// Implementation of the ChromePrompt Mojo interface.
class ChromePromptImpl : public chrome_cleaner::mojom::ChromePrompt {
 public:
  ChromePromptImpl(chrome_cleaner::mojom::ChromePromptRequest request,
                   base::Closure on_connection_closed);
  ~ChromePromptImpl() override;

  void PromptUser(
      std::vector<chrome_cleaner::mojom::UwSPtr> removable_uws_found,
      chrome_cleaner::mojom::ElevationStatus elevation_status,
      chrome_cleaner::mojom::ChromePrompt::PromptUserCallback callback)
      override;

 private:
  mojo::Binding<chrome_cleaner::mojom::ChromePrompt> binding_;

  DISALLOW_COPY_AND_ASSIGN(ChromePromptImpl);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SRT_CHROME_PROMPT_IMPL_H_
