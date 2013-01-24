// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/url_data_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "content/browser/webui/url_data_manager_backend.h"
#include "content/browser/webui/web_ui_data_source.h"
#include "content/browser/resource_context_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {

const char kURLDataManagerKeyName[] = "url_data_manager";

base::LazyInstance<base::Lock>::Leaky g_delete_lock = LAZY_INSTANCE_INITIALIZER;

ChromeURLDataManager* GetFromBrowserContext(BrowserContext* context) {
  if (!context->GetUserData(kURLDataManagerKeyName)) {
    context->SetUserData(kURLDataManagerKeyName,
                         new ChromeURLDataManager(context));
  }
  return static_cast<ChromeURLDataManager*>(
      context->GetUserData(kURLDataManagerKeyName));
}

// Invoked on the IO thread to do the actual adding of the DataSource.
static void AddDataSourceOnIOThread(
    content::ResourceContext* resource_context,
    scoped_refptr<URLDataSourceImpl> data_source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GetURLDataManagerForResourceContext(resource_context)->AddDataSource(
      data_source.get());
}

}  // namespace


// static
ChromeURLDataManager::URLDataSources* ChromeURLDataManager::data_sources_ =
    NULL;

ChromeURLDataManager::ChromeURLDataManager(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

ChromeURLDataManager::~ChromeURLDataManager() {
}

void ChromeURLDataManager::AddDataSource(URLDataSourceImpl* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AddDataSourceOnIOThread,
                 browser_context_->GetResourceContext(),
                 make_scoped_refptr(source)));
}

// static
void ChromeURLDataManager::DeleteDataSources() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  URLDataSources sources;
  {
    base::AutoLock lock(g_delete_lock.Get());
    if (!data_sources_)
      return;
    data_sources_->swap(sources);
  }
  for (size_t i = 0; i < sources.size(); ++i)
    delete sources[i];
}

// static
void ChromeURLDataManager::DeleteDataSource(
    const URLDataSourceImpl* data_source) {
  // Invoked when a DataSource is no longer referenced and needs to be deleted.
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    // We're on the UI thread, delete right away.
    delete data_source;
    return;
  }

  // We're not on the UI thread, add the DataSource to the list of DataSources
  // to delete.
  bool schedule_delete = false;
  {
    base::AutoLock lock(g_delete_lock.Get());
    if (!data_sources_)
      data_sources_ = new URLDataSources();
    schedule_delete = data_sources_->empty();
    data_sources_->push_back(data_source);
  }
  if (schedule_delete) {
    // Schedule a task to delete the DataSource back on the UI thread.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ChromeURLDataManager::DeleteDataSources));
  }
}

// static
void ChromeURLDataManager::AddDataSource(
    content::BrowserContext* browser_context,
    content::URLDataSource* source) {
  GetFromBrowserContext(browser_context)->
      AddDataSource(new URLDataSourceImpl(source->GetSource(), source));
}

// static
void ChromeURLDataManager::AddWebUIDataSource(
    content::BrowserContext* browser_context,
    content::WebUIDataSource* source) {
  ChromeWebUIDataSource* impl = static_cast<ChromeWebUIDataSource*>(source);
  GetFromBrowserContext(browser_context)->AddDataSource(impl);
}

// static
bool ChromeURLDataManager::IsScheduledForDeletion(
    const URLDataSourceImpl* data_source) {
  base::AutoLock lock(g_delete_lock.Get());
  if (!data_sources_)
    return false;
  return std::find(data_sources_->begin(), data_sources_->end(), data_source) !=
      data_sources_->end();
}

URLDataSourceImpl::URLDataSourceImpl(const std::string& source_name,
                                     content::URLDataSource* source)
    : source_name_(source_name),
      backend_(NULL),
      source_(source) {
}

URLDataSourceImpl::~URLDataSourceImpl() {
}

void URLDataSourceImpl::SendResponse(
    int request_id,
    base::RefCountedMemory* bytes) {
  // Take a ref-pointer on entry so byte->Release() will always get called.
  scoped_refptr<base::RefCountedMemory> bytes_ptr(bytes);
  if (ChromeURLDataManager::IsScheduledForDeletion(this)) {
    // We're scheduled for deletion. Servicing the request would result in
    // this->AddRef being invoked, even though the ref count is 0 and 'this' is
    // about to be deleted. If the AddRef were allowed through, when 'this' is
    // released it would be deleted again.
    //
    // This scenario occurs with DataSources that make history requests. Such
    // DataSources do a history query in |StartDataRequest| and the request is
    // live until the object is deleted (history requests don't up the ref
    // count). This means it's entirely possible for the DataSource to invoke
    // |SendResponse| between the time when there are no more refs and the time
    // when the object is deleted.
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&URLDataSourceImpl::SendResponseOnIOThread, this, request_id,
                 bytes_ptr));
}

void URLDataSourceImpl::SendResponseOnIOThread(
    int request_id,
    scoped_refptr<base::RefCountedMemory> bytes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (backend_)
    backend_->DataAvailable(request_id, bytes);
}
