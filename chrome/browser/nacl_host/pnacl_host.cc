// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/pnacl_host.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/nacl_host/nacl_browser.h"
#include "chrome/browser/nacl_host/pnacl_translation_cache.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {
static const base::FilePath::CharType kTranslationCacheDirectoryName[] =
    FILE_PATH_LITERAL("PnaclTranslationCache");
}

PnaclHost::PnaclHost()
    : cache_state_(CacheUninitialized), weak_factory_(this) {}

PnaclHost::~PnaclHost() {}

PnaclHost* PnaclHost::GetInstance() { return Singleton<PnaclHost>::get(); }

PnaclHost::PendingTranslation::PendingTranslation() {}
PnaclHost::PendingTranslation::~PendingTranslation() {}

/////////////////////////////////////// Initialization

static base::FilePath GetCachePath() {
  NaClBrowserDelegate* browser_delegate = NaClBrowser::GetDelegate();
  // Determine where the translation cache resides in the file system.  It
  // exists in Chrome's cache directory and is not tied to any specific
  // profile. If we fail, return an empty path.
  // Start by finding the user data directory.
  base::FilePath user_data_dir;
  if (!browser_delegate ||
      !browser_delegate->GetUserDirectory(&user_data_dir)) {
    return base::FilePath();
  }
  // The cache directory may or may not be the user data directory.
  base::FilePath cache_file_path;
  browser_delegate->GetCacheDirectory(&cache_file_path);

  // Append the base file name to the cache directory.
  return cache_file_path.Append(kTranslationCacheDirectoryName);
}

void PnaclHost::OnCacheInitialized(int error) {
  // If the cache was cleared before the load completed, ignore.
  if (cache_state_ == CacheReady)
    return;
  if (error != net::OK) {
    LOG(ERROR) << "PNaCl translation cache initalization failure: " << error
               << "\n";
  } else {
    cache_state_ = CacheReady;
  }
}

void PnaclHost::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::FilePath cache_path(GetCachePath());
  if (cache_path.empty() || cache_state_ != CacheUninitialized)
    return;
  disk_cache_.reset(new pnacl::PnaclTranslationCache());
  cache_state_ = CacheInitializing;
  disk_cache_->InitCache(
      cache_path,
      true,
      base::Bind(&PnaclHost::OnCacheInitialized, weak_factory_.GetWeakPtr()));
}

// Initialize using the in-memory backend, and manually set the temporary file
// directory instead of using the system directory.
void PnaclHost::InitForTest(base::FilePath temp_dir) {
  DCHECK(thread_checker_.CalledOnValidThread());
  disk_cache_.reset(new pnacl::PnaclTranslationCache());
  cache_state_ = CacheInitializing;
  temp_dir_ = temp_dir;
  disk_cache_->InitCache(
      temp_dir,
      true,  // Use in-memory backend
      base::Bind(&PnaclHost::OnCacheInitialized, weak_factory_.GetWeakPtr()));
}

///////////////////////////////////////// Temp files

// Create a temporary file on the blocking pool
IPC::PlatformFileForTransit PnaclHost::DoCreateTemporaryFile(
    base::ProcessHandle process_handle,
    base::FilePath temp_dir) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  base::FilePath file_path;
  bool rv = temp_dir.empty()
                ? file_util::CreateTemporaryFile(&file_path)
                : file_util::CreateTemporaryFileInDir(temp_dir, &file_path);

  if (!rv)
    return IPC::InvalidPlatformFileForTransit();

  base::PlatformFileError error;
  base::PlatformFile file_handle(base::CreatePlatformFile(
      file_path,
      base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_READ |
          base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_TEMPORARY |
          base::PLATFORM_FILE_DELETE_ON_CLOSE,
      NULL,
      &error));

  if (error != base::PLATFORM_FILE_OK)
    return IPC::InvalidPlatformFileForTransit();

  // Do any DuplicateHandle magic that is necessary first.
  return IPC::GetFileHandleForProcess(file_handle, process_handle, true);
}

void PnaclHost::CreateTemporaryFile(base::ProcessHandle process_handle,
                                    TempFileCallback cb) {
  if (!base::PostTaskAndReplyWithResult(
          BrowserThread::GetBlockingPool(),
          FROM_HERE,
          base::Bind(
              &PnaclHost::DoCreateTemporaryFile, process_handle, temp_dir_),
          cb)) {
    DCHECK(thread_checker_.CalledOnValidThread());
    cb.Run(IPC::InvalidPlatformFileForTransit());
  }
}

///////////////////////////////////////// GetNexeFd implementation

void PnaclHost::ReturnMiss(TranslationID id, IPC::PlatformFileForTransit fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PendingTranslationMap::iterator entry(pending_translations_.find(id));
  if (entry == pending_translations_.end()) {
    LOG(ERROR) << "PnaclHost::ReturnMiss: Failed to find TranslationID "
               << id.first << "," << id.second;
    return;
  }
  NexeFdCallback cb(entry->second.callback);
  if (fd == IPC::InvalidPlatformFileForTransit()) {
    pending_translations_.erase(entry);
  } else {
    entry->second.nexe_fd = fd;
  }
  cb.Run(fd, false);
}

void PnaclHost::GetNexeFd(int render_process_id,
                          base::ProcessHandle process_handle,
                          int render_view_id,
                          int pp_instance,
                          const nacl::PnaclCacheInfo& cache_info,
                          const NexeFdCallback& cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (cache_state_ != CacheReady)
    return;
  PendingTranslation pt;
  pt.render_view_id = render_view_id;
  pt.callback = cb;
  pt.cache_info = cache_info;

  pending_translations_[TranslationID(render_process_id, pp_instance)] = pt;

  CreateTemporaryFile(
      process_handle,
      base::Bind(&PnaclHost::ReturnMiss,
                 weak_factory_.GetWeakPtr(),
                 TranslationID(render_process_id, pp_instance)));
}

/////////////////// Cleanup

void PnaclHost::TranslationFinished(int render_process_id,
                                    int pp_instance,
                                    bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (cache_state_ != CacheReady)
    return;
  PendingTranslationMap::iterator it(pending_translations_.find(
      TranslationID(render_process_id, pp_instance)));
  if (it == pending_translations_.end()) {
    LOG(ERROR) << "TranslationFinished: TranslationID " << render_process_id
               << "," << pp_instance << " not found.";
  } else {
    pending_translations_.erase(it);
  }
}

void PnaclHost::RendererClosing(int render_process_id) {
  if (cache_state_ != CacheReady)
    return;
  DCHECK(thread_checker_.CalledOnValidThread());
  for (PendingTranslationMap::iterator it = pending_translations_.begin();
       it != pending_translations_.end();) {
    PendingTranslationMap::iterator to_erase(it++);
    if (to_erase->first.first == render_process_id) {
      // clean up the open files
      pending_translations_.erase(to_erase);
    }
  }
}
