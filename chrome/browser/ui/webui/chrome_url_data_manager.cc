// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_factory.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager_backend.h"
#include "content/public/browser/browser_thread.h"
#include "grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined (TOOLKIT_GTK)
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using content::BrowserThread;

static base::LazyInstance<base::Lock>::Leaky
    g_delete_lock = LAZY_INSTANCE_INITIALIZER;

// static
ChromeURLDataManager::DataSources* ChromeURLDataManager::data_sources_ = NULL;

// Invoked on the IO thread to do the actual adding of the DataSource.
static void AddDataSourceOnIOThread(
    const base::Callback<ChromeURLDataManagerBackend*(void)>& backend,
    scoped_refptr<ChromeURLDataManager::DataSource> data_source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  backend.Run()->AddDataSource(data_source.get());
}

ChromeURLDataManager::ChromeURLDataManager(
      const base::Callback<ChromeURLDataManagerBackend*(void)>& backend)
    : backend_(backend) {
}

ChromeURLDataManager::~ChromeURLDataManager() {
}

void ChromeURLDataManager::AddDataSource(DataSource* source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&AddDataSourceOnIOThread,
                 backend_, make_scoped_refptr(source)));
}

// static
void ChromeURLDataManager::DeleteDataSources() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DataSources sources;
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
    base::AutoLock lock(g_delete_lock.Get());
    if (!data_sources_)
      data_sources_ = new DataSources();
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
void ChromeURLDataManager::AddDataSource(Profile* profile, DataSource* source) {
  ChromeURLDataManagerFactory::GetForProfile(profile)->AddDataSource(source);
}

// static
bool ChromeURLDataManager::IsScheduledForDeletion(
    const DataSource* data_source) {
  base::AutoLock lock(g_delete_lock.Get());
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

void ChromeURLDataManager::DataSource::SendResponse(
    int request_id,
    base::RefCountedMemory* bytes) {
  // Take a ref-pointer on entry so byte->Release() will always get called.
  scoped_refptr<base::RefCountedMemory> bytes_ptr(bytes);
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
      base::Bind(&DataSource::SendResponseOnIOThread, this, request_id,
                 bytes_ptr));
}

MessageLoop* ChromeURLDataManager::DataSource::MessageLoopForRequestPath(
    const std::string& path) const {
  return message_loop_;
}

bool ChromeURLDataManager::DataSource::ShouldReplaceExistingSource() const {
  return true;
}

// static
void ChromeURLDataManager::DataSource::SetFontAndTextDirection(
    DictionaryValue* localized_strings) {
  int web_font_family_id = IDS_WEB_FONT_FAMILY;
  int web_font_size_id = IDS_WEB_FONT_SIZE;
#if defined(OS_WIN)
  // Vary font settings for Windows XP.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    web_font_family_id = IDS_WEB_FONT_FAMILY_XP;
    web_font_size_id = IDS_WEB_FONT_SIZE_XP;
  }
#endif

  std::string font_family = l10n_util::GetStringUTF8(web_font_family_id);

#if defined(TOOLKIT_GTK)
  // Use the system font on Linux/GTK. Keep the hard-coded font families as
  // backup in case for some crazy reason this one isn't available.
  font_family = ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::BaseFont).GetFontName() + ", " + font_family;
#endif

  localized_strings->SetString("fontfamily", font_family);
  localized_strings->SetString("fontsize",
      l10n_util::GetStringUTF8(web_font_size_id));
  localized_strings->SetString("textdirection",
      base::i18n::IsRTL() ? "rtl" : "ltr");
}

void ChromeURLDataManager::DataSource::SendResponseOnIOThread(
    int request_id,
    scoped_refptr<base::RefCountedMemory> bytes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (backend_)
    backend_->DataAvailable(request_id, bytes);
}
