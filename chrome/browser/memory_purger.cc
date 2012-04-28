// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_purger.h"

#include <set>

#include "base/allocator/allocator_extension.h"
#include "base/bind.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/resource_context.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserContext;
using content::BrowserThread;
using content::ResourceContext;

// PurgeMemoryHelper -----------------------------------------------------------

// This is a small helper class used to ensure that the objects we want to use
// on multiple threads are properly refed, so they don't get deleted out from
// under us.
class PurgeMemoryIOHelper
    : public base::RefCountedThreadSafe<PurgeMemoryIOHelper> {
 public:
  PurgeMemoryIOHelper() {
    safe_browsing_service_ = g_browser_process->safe_browsing_service();
  }

  void AddRequestContextGetter(
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);

  void PurgeMemoryOnIOThread();

 private:
  friend class base::RefCountedThreadSafe<PurgeMemoryIOHelper>;

  virtual ~PurgeMemoryIOHelper() {}

  typedef scoped_refptr<net::URLRequestContextGetter> RequestContextGetter;
  std::vector<RequestContextGetter> request_context_getters_;

  scoped_refptr<SafeBrowsingService> safe_browsing_service_;

  DISALLOW_COPY_AND_ASSIGN(PurgeMemoryIOHelper);
};

void PurgeMemoryIOHelper::AddRequestContextGetter(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter) {
  request_context_getters_.push_back(request_context_getter);
}

void PurgeMemoryIOHelper::PurgeMemoryOnIOThread() {
  // Ask ProxyServices to purge any memory they can (generally garbage in the
  // wrapped ProxyResolver's JS engine).
  for (size_t i = 0; i < request_context_getters_.size(); ++i) {
    request_context_getters_[i]->GetURLRequestContext()->proxy_service()->
        PurgeMemory();
  }

#if defined(ENABLE_SAFE_BROWSING)
  safe_browsing_service_->PurgeMemory();
#endif
}

// -----------------------------------------------------------------------------

// static
void MemoryPurger::PurgeAll() {
  PurgeBrowser();
  PurgeRenderers();

  // TODO(pkasting):
  // * Tell the plugin processes to release their free memory?  Other stuff?
  // * Enumerate what other processes exist and what to do for them.
}

// static
void MemoryPurger::PurgeBrowser() {
  // Dump the backing stores.
  content::RenderWidgetHost::RemoveAllBackingStores();

  // Per-profile cleanup.
  scoped_refptr<PurgeMemoryIOHelper> purge_memory_io_helper(
      new PurgeMemoryIOHelper());
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i) {
    purge_memory_io_helper->AddRequestContextGetter(
        make_scoped_refptr(profiles[i]->GetRequestContext()));

    // NOTE: Some objects below may be duplicates across profiles.  We could
    // conceivably put all these in sets and then iterate over the sets.

    // Unload all history backends (freeing memory used to cache sqlite).
    // Spinning up the history service is expensive, so we avoid doing it if it
    // hasn't been done already.
    HistoryService* history_service =
        profiles[i]->GetHistoryServiceWithoutCreating();
    if (history_service)
      history_service->UnloadBackend();

    // Unload all web databases (freeing memory used to cache sqlite).
    WebDataService* web_data_service =
        profiles[i]->GetWebDataServiceWithoutCreating();
    if (web_data_service)
      web_data_service->UnloadDatabase();

    BrowserContext::PurgeMemory(profiles[i]);
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PurgeMemoryIOHelper::PurgeMemoryOnIOThread,
                 purge_memory_io_helper.get()));

  // TODO(pkasting):
  // * Purge AppCache memory.  Not yet implemented sufficiently.
  // * Browser-side DatabaseTracker.  Not implemented sufficiently.

  // Tell our allocator to release any free pages it's still holding.
  //
  // TODO(pkasting): A lot of the above calls kick off actions on other threads.
  // Maybe we should find a way to avoid calling this until those actions
  // complete?
  base::allocator::ReleaseFreeMemory();
}

// static
void MemoryPurger::PurgeRenderers() {
  // Direct all renderers to free everything they can.
  //
  // Concern: Telling a bunch of renderer processes to destroy their data may
  // cause them to page everything in to do it, which could take a lot of time/
  // cause jank.
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance())
    PurgeRendererForHost(i.GetCurrentValue());
}

// static
void MemoryPurger::PurgeRendererForHost(content::RenderProcessHost* host) {
  // Direct the renderer to free everything it can.
  host->Send(new ChromeViewMsg_PurgeMemory());
}
