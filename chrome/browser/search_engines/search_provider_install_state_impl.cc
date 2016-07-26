// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/search_provider_install_state_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/google/google_url_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/search_provider.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "services/shell/public/cpp/interface_registry.h"
#include "url/gurl.h"

using content::BrowserThread;

SearchProviderInstallStateImpl::SearchProviderInstallStateImpl(
    int render_process_id,
    Profile* profile)
    : provider_data_(TemplateURLServiceFactory::GetForProfile(profile),
                     UIThreadSearchTermsData(profile).GoogleBaseURLValue(),
                     GoogleURLTrackerFactory::GetForProfile(profile),
                     content::RenderProcessHost::FromID(render_process_id)),
      is_off_the_record_(profile->IsOffTheRecord()),
      binding_(this),
      weak_factory_(this) {
  // This is initialized by RenderProcessHostImpl. Do not add any non-trivial
  // initialization here. Instead do it lazily when required.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SearchProviderInstallStateImpl::~SearchProviderInstallStateImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

// static
void SearchProviderInstallStateImpl::Create(
    int render_process_id,
    Profile* profile,
    chrome::mojom::SearchProviderInstallStateRequest request) {
  // BindOnIOThread takes ownership on the impl object:
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SearchProviderInstallStateImpl::BindOnIOThread,
                 base::Unretained(new SearchProviderInstallStateImpl(
                     render_process_id, profile)),
                 base::Passed(&request)));
}

void SearchProviderInstallStateImpl::BindOnIOThread(
    chrome::mojom::SearchProviderInstallStateRequest request) {
  binding_.Bind(std::move(request));
}

chrome::mojom::InstallState
SearchProviderInstallStateImpl::GetSearchProviderInstallState(
    const GURL& page_location,
    const GURL& requested_host) {
  GURL requested_origin = requested_host.GetOrigin();

  // Do the security check before any others to avoid information leaks.
  if (page_location.GetOrigin() != requested_origin)
    return chrome::mojom::InstallState::DENIED;

  // In incognito mode, no search information is exposed. (This check must be
  // done after the security check or else a web site can detect that the
  // user is in incognito mode just by doing a cross origin request.)
  if (is_off_the_record_)
    return chrome::mojom::InstallState::NOT_INSTALLED;

  switch (provider_data_.GetInstallState(requested_origin)) {
    case SearchProviderInstallData::NOT_INSTALLED:
      return chrome::mojom::InstallState::NOT_INSTALLED;

    case SearchProviderInstallData::INSTALLED_BUT_NOT_DEFAULT:
      return chrome::mojom::InstallState::INSTALLED_BUT_NOT_DEFAULT;

    case SearchProviderInstallData::INSTALLED_AS_DEFAULT:
      return chrome::mojom::InstallState::INSTALLED_AS_DEFAULT;
  }

  NOTREACHED();
  return chrome::mojom::InstallState::NOT_INSTALLED;
}

void SearchProviderInstallStateImpl::GetInstallState(
    const GURL& page_location,
    const GURL& requested_host,
    const GetInstallStateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  provider_data_.CallWhenLoaded(base::Bind(
      &SearchProviderInstallStateImpl::ReplyWithProviderInstallState,
      weak_factory_.GetWeakPtr(), page_location, requested_host, callback));
}

void SearchProviderInstallStateImpl::ReplyWithProviderInstallState(
    const GURL& page_location,
    const GURL& requested_host,
    const GetInstallStateCallback& callback) {
  chrome::mojom::InstallState install_state =
      GetSearchProviderInstallState(page_location, requested_host);

  callback.Run(install_state);
}
