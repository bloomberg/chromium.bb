// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"

#include <vector>

#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/ref_counted_memory.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager_backend.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

// static
base::Lock ChromeURLDataManager::delete_lock_;

// static
ChromeURLDataManager::DataSources* ChromeURLDataManager::data_sources_ = NULL;

// Invoked on the IO thread to do the actual adding of the DataSource.
static void AddDataSourceOnIOThread(
    scoped_refptr<URLRequestContextGetter> context_getter,
    scoped_refptr<ChromeURLDataManager::DataSource> data_source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  static_cast<ChromeURLRequestContext*>(
      context_getter->GetURLRequestContext())->
      GetChromeURLDataManagerBackend()->AddDataSource(data_source.get());
}

ChromeURLDataManager::ChromeURLDataManager(Profile* profile)
    : profile_(profile) {
}

ChromeURLDataManager::~ChromeURLDataManager() {
}

void ChromeURLDataManager::AddDataSource(DataSource* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registered_source_names_.insert(source->source_name());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableFunction(AddDataSourceOnIOThread,
                          make_scoped_refptr(profile_->GetRequestContext()),
                          make_scoped_refptr(source)));
}

bool ChromeURLDataManager::IsRegistered(const std::string& name) {
  return registered_source_names_.find(name) != registered_source_names_.end();
}

// static
void ChromeURLDataManager::DeleteDataSources() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DataSources sources;
  {
    base::AutoLock lock(delete_lock_);
    if (!data_sources_)
      return;
    data_sources_->swap(sources);
  }
  for (size_t i = 0; i < sources.size(); ++i)
    delete sources[i];
}

// static
void ChromeURLDataManager::DeleteDataSource(const DataSource* data_source) {
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
    base::AutoLock lock(delete_lock_);
    if (!data_sources_)
      data_sources_ = new DataSources();
    schedule_delete = data_sources_->empty();
    data_sources_->push_back(data_source);
  }
  if (schedule_delete) {
    // Schedule a task to delete the DataSource back on the UI thread.
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            NewRunnableFunction(
                                &ChromeURLDataManager::DeleteDataSources));
  }
}

// static
bool ChromeURLDataManager::IsScheduledForDeletion(
    const DataSource* data_source) {
  base::AutoLock lock(delete_lock_);
  if (!data_sources_)
    return false;
  return std::find(data_sources_->begin(), data_sources_->end(), data_source) !=
      data_sources_->end();
}

ChromeURLDataManager::DataSource::DataSource(const std::string& source_name,
                                             MessageLoop* message_loop)
    : source_name_(source_name),
      message_loop_(message_loop),
      backend_(NULL) {
}

ChromeURLDataManager::DataSource::~DataSource() {
}

void ChromeURLDataManager::DataSource::SendResponse(int request_id,
                                                    RefCountedMemory* bytes) {
  if (IsScheduledForDeletion(this)) {
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
      NewRunnableMethod(this, &DataSource::SendResponseOnIOThread,
                        request_id, make_scoped_refptr(bytes)));
}

MessageLoop* ChromeURLDataManager::DataSource::MessageLoopForRequestPath(
    const std::string& path) const {
  return message_loop_;
}

// static
void ChromeURLDataManager::DataSource::SetFontAndTextDirection(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("fontfamily",
      l10n_util::GetStringUTF16(IDS_WEB_FONT_FAMILY));

  int web_font_size_id = IDS_WEB_FONT_SIZE;
#if defined(OS_WIN)
  // Some fonts used for some languages changed a lot in terms of the font
  // metric in Vista. So, we need to use different size before Vista.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    web_font_size_id = IDS_WEB_FONT_SIZE_XP;
#endif
  localized_strings->SetString("fontsize",
      l10n_util::GetStringUTF16(web_font_size_id));

  localized_strings->SetString("textdirection",
      base::i18n::IsRTL() ? "rtl" : "ltr");
}

void ChromeURLDataManager::DataSource::SendResponseOnIOThread(
    int request_id,
    scoped_refptr<RefCountedMemory> bytes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (backend_)
    backend_->DataAvailable(request_id, bytes);
}
