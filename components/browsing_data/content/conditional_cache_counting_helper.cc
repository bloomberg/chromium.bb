// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/conditional_cache_counting_helper.h"

#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace browsing_data {

// static.
ConditionalCacheCountingHelper* ConditionalCacheCountingHelper::CreateForRange(
    content::StoragePartition* storage_partition,
    base::Time begin_time,
    base::Time end_time) {
  return new ConditionalCacheCountingHelper(
      begin_time, end_time, storage_partition->GetURLRequestContext(),
      storage_partition->GetMediaURLRequestContext());
}

ConditionalCacheCountingHelper::ConditionalCacheCountingHelper(
    base::Time begin_time,
    base::Time end_time,
    net::URLRequestContextGetter* main_context_getter,
    net::URLRequestContextGetter* media_context_getter)
    : calculation_result_(0),
      begin_time_(begin_time),
      end_time_(end_time),
      is_finished_(false),
      main_context_getter_(main_context_getter),
      media_context_getter_(media_context_getter),
      next_cache_state_(CacheState::NONE),
      cache_(nullptr),
      iterator_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ConditionalCacheCountingHelper::~ConditionalCacheCountingHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

base::WeakPtr<ConditionalCacheCountingHelper>
ConditionalCacheCountingHelper::CountAndDestroySelfWhenFinished(
    const CacheCountCallback& result_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!result_callback.is_null());
  result_callback_ = result_callback;
  calculation_result_ = 0;
  is_upper_limit_ = false;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ConditionalCacheCountingHelper::CountHttpCacheOnIOThread,
                 base::Unretained(this)));
  return weak_ptr_factory_.GetWeakPtr();
}

bool ConditionalCacheCountingHelper::IsFinished() {
  return is_finished_;
}

void ConditionalCacheCountingHelper::Finished() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_finished_);
  is_finished_ = true;
  result_callback_.Run(is_upper_limit_, calculation_result_);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void ConditionalCacheCountingHelper::CountHttpCacheOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  next_cache_state_ = CacheState::NONE;
  DCHECK_EQ(CacheState::NONE, next_cache_state_);
  DCHECK(main_context_getter_.get());
  DCHECK(media_context_getter_.get());

  next_cache_state_ = CacheState::CREATE_MAIN;
  DoCountCache(net::OK);
}

// The expected state sequence is CacheState::NONE --> CacheState::CREATE_MAIN
// --> CacheState::PROCESS_MAIN --> CacheState::CREATE_MEDIA -->
// CacheState::PROCESS_MEDIA --> CacheState::DONE.
// On error, we jump directly to CacheState::DONE.
void ConditionalCacheCountingHelper::DoCountCache(int rv) {
  DCHECK_NE(CacheState::NONE, next_cache_state_);
  while (rv != net::ERR_IO_PENDING && next_cache_state_ != CacheState::NONE) {
    // On error, finish and return the error code. A valid result value might
    // be of two types - either net::OK from the CREATE states, or the result
    // of calculation from the PROCESS states. Since net::OK == 0, it is valid
    // to simply add the value to the final calculation result.
    if (rv < 0) {
      calculation_result_ = rv;
      next_cache_state_ = CacheState::DONE;
    } else {
      DCHECK_EQ(0, net::OK);
      calculation_result_ += rv;
    }

    switch (next_cache_state_) {
      case CacheState::CREATE_MAIN:
      case CacheState::CREATE_MEDIA: {
        // Get a pointer to the cache.
        net::URLRequestContextGetter* getter =
            (next_cache_state_ == CacheState::CREATE_MAIN)
                ? main_context_getter_.get()
                : media_context_getter_.get();
        net::HttpCache* http_cache = getter->GetURLRequestContext()
                                         ->http_transaction_factory()
                                         ->GetCache();

        next_cache_state_ = (next_cache_state_ == CacheState::CREATE_MAIN)
                                ? CacheState::COUNT_MAIN
                                : CacheState::COUNT_MEDIA;

        rv = http_cache->GetBackend(
            &cache_, base::Bind(&ConditionalCacheCountingHelper::DoCountCache,
                                base::Unretained(this)));
        break;
      }
      case CacheState::COUNT_MAIN:
      case CacheState::COUNT_MEDIA: {
        next_cache_state_ = (next_cache_state_ == CacheState::COUNT_MAIN)
                                ? CacheState::CREATE_MEDIA
                                : CacheState::DONE;

        // |cache_| can be null if it cannot be initialized.
        if (cache_) {
          if (begin_time_.is_null() && end_time_.is_max()) {
            rv = cache_->CalculateSizeOfAllEntries(
                base::Bind(&ConditionalCacheCountingHelper::DoCountCache,
                           base::Unretained(this)));
          } else {
            rv = cache_->CalculateSizeOfEntriesBetween(
                begin_time_, end_time_,
                base::Bind(&ConditionalCacheCountingHelper::DoCountCache,
                           base::Unretained(this)));
            if (rv == net::ERR_NOT_IMPLEMENTED) {
              is_upper_limit_ = true;
              rv = cache_->CalculateSizeOfAllEntries(
                  base::Bind(&ConditionalCacheCountingHelper::DoCountCache,
                             base::Unretained(this)));
            }
          }
          cache_ = nullptr;
        }
        break;
      }
      case CacheState::DONE: {
        cache_ = nullptr;
        next_cache_state_ = CacheState::NONE;
        // Notify the UI thread that we are done.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&ConditionalCacheCountingHelper::Finished,
                       base::Unretained(this)));
        return;
      }
      case CacheState::NONE: {
        NOTREACHED() << "bad state";
        return;
      }
    }
  }
}

}  // namespace browsing_data
