// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_STATE_IMPL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_STATE_IMPL_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/search_engines/search_provider_install_data.h"
#include "chrome/common/search_provider.mojom.h"
#include "content/public/browser/browser_message_filter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

class GURL;
class Profile;

namespace content {
class RenderProcessHost;
}  // namespace content

// Handles messages regarding search provider install state on the I/O thread.
class SearchProviderInstallStateImpl
    : public chrome::mojom::SearchProviderInstallState {
 public:
  // Unlike the other methods, the constructor must be called on the UI thread.
  SearchProviderInstallStateImpl(int render_process_id, Profile* profile);
  ~SearchProviderInstallStateImpl() override;

  static void Create(int render_process_id,
                     Profile* profile,
                     chrome::mojom::SearchProviderInstallStateRequest request);

 private:
  void BindOnIOThread(chrome::mojom::SearchProviderInstallStateRequest request);

  // Figures out the install state for the search provider.
  chrome::mojom::InstallState GetSearchProviderInstallState(
      const GURL& page_location,
      const GURL& requested_host);

  // Starts handling the message requesting the search provider install state.
  // chrome::mojom::SearchProviderInstallState override.
  void GetInstallState(const GURL& page_location,
                       const GURL& requested_host,
                       const GetInstallStateCallback& callback) override;

  // Sends the reply message about the search provider install state.
  void ReplyWithProviderInstallState(const GURL& page_location,
                                     const GURL& requested_host,
                                     const GetInstallStateCallback& callback);

  // Used to do a load and get information about install states.
  SearchProviderInstallData provider_data_;

  // Copied from the profile since the profile can't be accessed on the I/O
  // thread.
  const bool is_off_the_record_;

  mojo::StrongBinding<chrome::mojom::SearchProviderInstallState> binding_;

  // Used to schedule invocations of ReplyWithProviderInstallState.
  base::WeakPtrFactory<SearchProviderInstallStateImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderInstallStateImpl);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_STATE_IMPL_H_
