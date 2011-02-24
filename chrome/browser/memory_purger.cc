// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory_purger.h"

#include <set>

#include "base/threading/thread.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "content/browser/renderer_host/backing_store_manager.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "net/proxy/proxy_resolver.h"
#include "net/url_request/url_request_context.h"
#include "third_party/tcmalloc/chromium/src/google/malloc_extension.h"
#include "v8/include/v8.h"

// PurgeMemoryHelper -----------------------------------------------------------

// This is a small helper class used to ensure that the objects we want to use
// on multiple threads are properly refed, so they don't get deleted out from
// under us.
class PurgeMemoryIOHelper
    : public base::RefCountedThreadSafe<PurgeMemoryIOHelper> {
 public:
  explicit PurgeMemoryIOHelper(SafeBrowsingService* safe_browsing_service)
      : safe_browsing_service_(safe_browsing_service) {
  }

  void AddRequestContextGetter(
      scoped_refptr<URLRequestContextGetter> request_context_getter);

  void PurgeMemoryOnIOThread();

 private:
  typedef scoped_refptr<URLRequestContextGetter> RequestContextGetter;
  typedef std::set<RequestContextGetter> RequestContextGetters;

  RequestContextGetters request_context_getters_;
  scoped_refptr<SafeBrowsingService> safe_browsing_service_;

  DISALLOW_COPY_AND_ASSIGN(PurgeMemoryIOHelper);
};

void PurgeMemoryIOHelper::AddRequestContextGetter(
    scoped_refptr<URLRequestContextGetter> request_context_getter) {
  request_context_getters_.insert(request_context_getter);
}

void PurgeMemoryIOHelper::PurgeMemoryOnIOThread() {
  // Ask ProxyServices to purge any memory they can (generally garbage in the
  // wrapped ProxyResolver's JS engine).
  for (RequestContextGetters::const_iterator i(
           request_context_getters_.begin());
       i != request_context_getters_.end(); ++i)
    (*i)->GetURLRequestContext()->proxy_service()->PurgeMemory();

  // Close the Safe Browsing database, freeing memory used to cache sqlite as
  // well as a number of in-memory structures.
  safe_browsing_service_->CloseDatabase();

  // The appcache service listens for this notification.
  NotificationService::current()->Notify(
      NotificationType::PURGE_MEMORY,
      Source<void>(NULL),
      NotificationService::NoDetails());
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
  BackingStoreManager::RemoveAllBackingStores();

  // Per-profile cleanup.
  scoped_refptr<PurgeMemoryIOHelper> purge_memory_io_helper(
      new PurgeMemoryIOHelper(g_browser_process->resource_dispatcher_host()->
          safe_browsing_service()));
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  for (ProfileManager::iterator i(profile_manager->begin());
       i != profile_manager->end(); ++i) {
    Profile* profile = *i;
    purge_memory_io_helper->AddRequestContextGetter(
        make_scoped_refptr(profile->GetRequestContext()));

    // NOTE: Some objects below may be duplicates across profiles.  We could
    // conceivably put all these in sets and then iterate over the sets.

    // Unload all history backends (freeing memory used to cache sqlite).
    // Spinning up the history service is expensive, so we avoid doing it if it
    // hasn't been done already.
    HistoryService* history_service =
        profile->GetHistoryServiceWithoutCreating();
    if (history_service)
      history_service->UnloadBackend();

    // Unload all web databases (freeing memory used to cache sqlite).
    WebDataService* web_data_service =
        profile->GetWebDataServiceWithoutCreating();
    if (web_data_service)
      web_data_service->UnloadDatabase();

    // Ask all WebKitContexts to purge memory (freeing memory used to cache
    // the LocalStorage sqlite DB).  WebKitContext creation is basically free so
    // we don't bother with a "...WithoutCreating()" function.
    profile->GetWebKitContext()->PurgeMemory();
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(purge_memory_io_helper.get(),
                        &PurgeMemoryIOHelper::PurgeMemoryOnIOThread));

  // TODO(pkasting):
  // * Purge AppCache memory.  Not yet implemented sufficiently.
  // * Browser-side DatabaseTracker.  Not implemented sufficiently.

#if (defined(OS_WIN) || defined(OS_LINUX)) && defined(USE_TCMALLOC)
  // Tell tcmalloc to release any free pages it's still holding.
  //
  // TODO(pkasting): A lot of the above calls kick off actions on other threads.
  // Maybe we should find a way to avoid calling this until those actions
  // complete?
  MallocExtension::instance()->ReleaseFreeMemory();
#endif
}

// static
void MemoryPurger::PurgeRenderers() {
  // Direct all renderers to free everything they can.
  //
  // Concern: Telling a bunch of renderer processes to destroy their data may
  // cause them to page everything in to do it, which could take a lot of time/
  // cause jank.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance())
    PurgeRendererForHost(i.GetCurrentValue());
}

// static
void MemoryPurger::PurgeRendererForHost(RenderProcessHost* host) {
  // Direct the renderer to free everything it can.
  host->Send(new ViewMsg_PurgeMemory());
}
