// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/delta_file_service.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/android/history_report/delta_file_backend_leveldb.h"
#include "chrome/browser/android/history_report/delta_file_commons.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace {

void DoAddPage(history_report::DeltaFileBackend* backend, const GURL& url) {
  backend->PageAdded(url);
}

void DoDeletePage(history_report::DeltaFileBackend* backend, const GURL& url) {
  backend->PageDeleted(url);
}

void DoTrim(history_report::DeltaFileBackend* backend,
            int64 lower_bound,
            base::WaitableEvent* finished,
            int64* result) {
  *result = backend->Trim(lower_bound);
  finished->Signal();
}

void DoQuery(
    history_report::DeltaFileBackend* backend,
    int64 last_seq_no,
    int32 limit,
    base::WaitableEvent* finished,
    scoped_ptr<std::vector<history_report::DeltaFileEntryWithData> >* result) {
  *result = backend->Query(last_seq_no, limit).Pass();
  finished->Signal();
}

void DoRecreate(history_report::DeltaFileBackend* backend,
                const std::vector<std::string>& urls,
                base::WaitableEvent* finished,
                bool* result) {
  *result = backend->Recreate(urls);
  finished->Signal();
}

void DoClear(history_report::DeltaFileBackend* backend) {
  backend->Clear();
}

void DoDump(history_report::DeltaFileBackend* backend,
                base::WaitableEvent* finished,
                std::string* result) {
  result->append(backend->Dump());
  finished->Signal();
}

}  // namespace

namespace history_report {

using content::BrowserThread;

DeltaFileService::DeltaFileService(const base::FilePath& dir)
    : worker_pool_token_(BrowserThread::GetBlockingPool()->GetSequenceToken()),
      delta_file_backend_(new DeltaFileBackend(dir)) {
}

DeltaFileService::~DeltaFileService() {}

void DeltaFileService::PageAdded(const GURL& url) {
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  pool->PostSequencedWorkerTaskWithShutdownBehavior(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&DoAddPage,
                 base::Unretained(delta_file_backend_.get()),
                 url),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

void DeltaFileService::PageDeleted(const GURL& url) {
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  pool->PostSequencedWorkerTaskWithShutdownBehavior(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&DoDeletePage,
                 base::Unretained(delta_file_backend_.get()),
                 url),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

int64 DeltaFileService::Trim(int64 lower_bound) {
  int64 result;
  base::WaitableEvent finished(false, false);
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  pool->PostSequencedWorkerTaskWithShutdownBehavior(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&DoTrim,
                 base::Unretained(delta_file_backend_.get()),
                 lower_bound,
                 base::Unretained(&finished),
                 base::Unretained(&result)),
      base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  finished.Wait();
  return result;
}

scoped_ptr<std::vector<DeltaFileEntryWithData> > DeltaFileService::Query(
    int64 last_seq_no,
    int32 limit) {
  scoped_ptr<std::vector<DeltaFileEntryWithData> > result;
  base::WaitableEvent finished(false, false);
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  pool->PostSequencedWorkerTaskWithShutdownBehavior(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&DoQuery,
                 base::Unretained(delta_file_backend_.get()),
                 last_seq_no,
                 limit,
                 base::Unretained(&finished),
                 base::Unretained(&result)),
      base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  finished.Wait();
  return result.Pass();
}

bool DeltaFileService::Recreate(const std::vector<std::string>& urls) {
  bool result = false;
  base::WaitableEvent finished(false, false);
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  pool->PostSequencedWorkerTaskWithShutdownBehavior(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&DoRecreate,
                 base::Unretained(delta_file_backend_.get()),
                 urls,
                 base::Unretained(&finished),
                 base::Unretained(&result)),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
  finished.Wait();
  return result;
}

void DeltaFileService::Clear() {
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  pool->PostSequencedWorkerTaskWithShutdownBehavior(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&DoClear,
                 base::Unretained(delta_file_backend_.get())),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
}

std::string DeltaFileService::Dump() {
  std::string dump;
  base::WaitableEvent finished(false, false);
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  pool->PostSequencedWorkerTaskWithShutdownBehavior(
      worker_pool_token_,
      FROM_HERE,
      base::Bind(&DoDump,
                 base::Unretained(delta_file_backend_.get()),
                 base::Unretained(&finished),
                 base::Unretained(&dump)),
      base::SequencedWorkerPool::BLOCK_SHUTDOWN);
  finished.Wait();
  return dump;
}

}  // namespace history_report
