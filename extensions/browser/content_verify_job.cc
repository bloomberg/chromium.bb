// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verify_job.h"

#include <algorithm>

#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/timer/elapsed_timer.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_hash_reader.h"

namespace extensions {

namespace {

ContentVerifyJob::TestDelegate* g_test_delegate = NULL;
ContentVerifyJob::TestObserver* g_content_verify_job_test_observer = NULL;

class ScopedElapsedTimer {
 public:
  explicit ScopedElapsedTimer(base::TimeDelta* total) : total_(total) {
    DCHECK(total_);
  }

  ~ScopedElapsedTimer() { *total_ += timer.Elapsed(); }

 private:
  // Some total amount of time we should add our elapsed time to at
  // destruction.
  base::TimeDelta* total_;

  // A timer for how long this object has been alive.
  base::ElapsedTimer timer;
};

}  // namespace

ContentVerifyJob::ContentVerifyJob(ContentHashReader* hash_reader,
                                   FailureCallback failure_callback)
    : done_reading_(false),
      hashes_ready_(false),
      total_bytes_read_(0),
      current_block_(0),
      current_hash_byte_count_(0),
      hash_reader_(hash_reader),
      failure_callback_(std::move(failure_callback)),
      failed_(false) {}

ContentVerifyJob::~ContentVerifyJob() {
  UMA_HISTOGRAM_COUNTS("ExtensionContentVerifyJob.TimeSpentUS",
                       time_spent_.InMicroseconds());
}

void ContentVerifyJob::Start() {
  base::AutoLock auto_lock(lock_);
  if (g_content_verify_job_test_observer)
    g_content_verify_job_test_observer->JobStarted(
        hash_reader_->extension_id(), hash_reader_->relative_path());
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&ContentHashReader::Init, hash_reader_),
      base::Bind(&ContentVerifyJob::OnHashesReady, this));
}

void ContentVerifyJob::BytesRead(int count, const char* data) {
  ScopedElapsedTimer timer(&time_spent_);
  base::AutoLock auto_lock(lock_);
  if (failed_)
    return;
  if (g_test_delegate) {
    FailureReason reason =
        g_test_delegate->BytesRead(hash_reader_->extension_id(), count, data);
    if (reason != NONE)
      DispatchFailureCallback(reason);
    return;
  }
  if (!hashes_ready_) {
    queue_.append(data, count);
    return;
  }
  DCHECK_GE(count, 0);
  int bytes_added = 0;

  while (bytes_added < count) {
    if (current_block_ >= hash_reader_->block_count())
      return DispatchFailureCallback(HASH_MISMATCH);

    if (!current_hash_) {
      current_hash_byte_count_ = 0;
      current_hash_ = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    }
    // Compute how many bytes we should hash, and add them to the current hash.
    int bytes_to_hash =
        std::min(hash_reader_->block_size() - current_hash_byte_count_,
                 count - bytes_added);
    DCHECK_GT(bytes_to_hash, 0);
    current_hash_->Update(data + bytes_added, bytes_to_hash);
    bytes_added += bytes_to_hash;
    current_hash_byte_count_ += bytes_to_hash;
    total_bytes_read_ += bytes_to_hash;

    // If we finished reading a block worth of data, finish computing the hash
    // for it and make sure the expected hash matches.
    if (current_hash_byte_count_ == hash_reader_->block_size() &&
        !FinishBlock()) {
      DispatchFailureCallback(HASH_MISMATCH);
      return;
    }
  }
}

void ContentVerifyJob::DoneReading() {
  ScopedElapsedTimer timer(&time_spent_);
  base::AutoLock auto_lock(lock_);
  if (failed_)
    return;
  if (g_test_delegate) {
    FailureReason reason =
        g_test_delegate->DoneReading(hash_reader_->extension_id());
    if (reason != NONE)
      DispatchFailureCallback(reason);
    return;
  }
  done_reading_ = true;
  if (hashes_ready_) {
    if (!FinishBlock()) {
      DispatchFailureCallback(HASH_MISMATCH);
    } else if (g_content_verify_job_test_observer) {
      g_content_verify_job_test_observer->JobFinished(
          hash_reader_->extension_id(), hash_reader_->relative_path(), NONE);
    }
  }
}

bool ContentVerifyJob::FinishBlock() {
  if (current_hash_byte_count_ == 0) {
    if (!done_reading_ ||
        // If we have checked all blocks already, then nothing else to do here.
        current_block_ == hash_reader_->block_count()) {
      return true;
    }
  }
  if (!current_hash_) {
    // This happens when we fail to read the resource. Compute empty content's
    // hash in this case.
    current_hash_ = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  }
  std::string final(crypto::kSHA256Length, 0);
  current_hash_->Finish(base::string_as_array(& final), final.size());
  current_hash_.reset();
  current_hash_byte_count_ = 0;

  int block = current_block_++;

  const std::string* expected_hash = NULL;
  if (!hash_reader_->GetHashForBlock(block, &expected_hash) ||
      *expected_hash != final) {
    return false;
  }

  return true;
}

void ContentVerifyJob::OnHashesReady(bool success) {
  if (!success && !g_test_delegate) {
    // TODO(lazyboy): Make ContentHashReader::Init return an enum instead of
    // bool. This should make the following checks on |hash_reader_| easier
    // to digest and will avoid future bugs from creeping up.
    if (!hash_reader_->has_content_hashes()) {
      DispatchFailureCallback(MISSING_ALL_HASHES);
      return;
    }

    if (hash_reader_->file_missing_from_verified_contents()) {
      // Ignore verification of non-existent resources.
      if (g_content_verify_job_test_observer) {
        g_content_verify_job_test_observer->JobFinished(
            hash_reader_->extension_id(), hash_reader_->relative_path(), NONE);
      }
      return;
    }
    DispatchFailureCallback(NO_HASHES_FOR_FILE);
    return;
  }

  hashes_ready_ = true;
  if (!queue_.empty()) {
    std::string tmp;
    queue_.swap(tmp);
    BytesRead(tmp.size(), base::string_as_array(&tmp));
  }
  if (done_reading_) {
    ScopedElapsedTimer timer(&time_spent_);
    if (!FinishBlock()) {
      DispatchFailureCallback(HASH_MISMATCH);
    } else if (g_content_verify_job_test_observer) {
      g_content_verify_job_test_observer->JobFinished(
          hash_reader_->extension_id(), hash_reader_->relative_path(), NONE);
    }
  }
}

// static
void ContentVerifyJob::SetDelegateForTests(TestDelegate* delegate) {
  DCHECK(delegate == nullptr || g_test_delegate == nullptr)
      << "SetDelegateForTests does not support interleaving. Delegates should "
      << "be set and then cleared one at a time.";
  g_test_delegate = delegate;
}

// static
void ContentVerifyJob::SetObserverForTests(TestObserver* observer) {
  g_content_verify_job_test_observer = observer;
}

void ContentVerifyJob::DispatchFailureCallback(FailureReason reason) {
  DCHECK(!failed_);
  failed_ = true;
  if (!failure_callback_.is_null()) {
    VLOG(1) << "job failed for " << hash_reader_->extension_id() << " "
            << hash_reader_->relative_path().MaybeAsASCII()
            << " reason:" << reason;
    std::move(failure_callback_).Run(reason);
  }
  if (g_content_verify_job_test_observer) {
    g_content_verify_job_test_observer->JobFinished(
        hash_reader_->extension_id(), hash_reader_->relative_path(), reason);
  }
}

}  // namespace extensions
