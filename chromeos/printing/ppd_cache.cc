// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_cache.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_parser.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/printing/printing_constants.h"
#include "crypto/sha2.h"
#include "net/base/io_buffer.h"
#include "net/filter/gzip_header.h"

namespace chromeos {
namespace printing {
namespace {

class PpdCacheImpl : public PpdCache {
 public:
  PpdCacheImpl(const base::FilePath& cache_base_dir,
               const scoped_refptr<base::SequencedTaskRunner>& disk_task_runner)
      : cache_base_dir_(cache_base_dir), disk_task_runner_(disk_task_runner) {}

  // Public API functions.
  void Find(const std::string& key, const FindCallback& cb) override {
    // Ensure the cache lives until the op is over.
    AddRef();
    ++inflight_ops_;
    disk_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PpdCacheImpl::FindImpl, this, key,
                              base::SequencedTaskRunnerHandle::Get(), cb));
  }

  // Store the given contents at the given key.  If cb is non-null, it will
  // be invoked on completion.
  void Store(const std::string& key,
             const std::string& contents,
             const base::Callback<void()>& cb) override {
    AddRef();
    ++inflight_ops_;
    disk_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PpdCacheImpl::StoreImpl, this, key, contents,
                              base::SequencedTaskRunnerHandle::Get(), cb));
  }

  bool Idle() const override { return inflight_ops_ == 0; }

 private:
  ~PpdCacheImpl() override {}

  // If the cache doesn't already exist, create it.
  void MaybeCreateCache() {
    if (!base::PathExists(cache_base_dir_)) {
      base::CreateDirectory(cache_base_dir_);
    }
  }

  // Find implementation, runs on the i/o thread.
  void FindImpl(const std::string& key,
                const scoped_refptr<base::SequencedTaskRunner>& callback_runner,
                const FindCallback& cb) {
    base::ThreadRestrictions::AssertIOAllowed();
    FindResult result;
    result.success = false;
    MaybeCreateCache();
    base::File file(FilePathForKey(key),
                    base::File::FLAG_OPEN | base::File::FLAG_READ);

    if (file.IsValid()) {
      int64_t len = file.GetLength();
      if (len >= static_cast<int64_t>(crypto::kSHA256Length) &&
          len <= static_cast<int64_t>(kMaxPpdSizeBytes) +
                     static_cast<int64_t>(crypto::kSHA256Length)) {
        std::unique_ptr<char[]> buf(new char[len]);
        if (file.ReadAtCurrentPos(buf.get(), len) == len) {
          base::StringPiece contents(buf.get(), len - crypto::kSHA256Length);
          base::StringPiece checksum(buf.get() + len - crypto::kSHA256Length,
                                     crypto::kSHA256Length);
          if (crypto::SHA256HashString(contents) == checksum) {
            base::File::Info info;
            if (file.GetInfo(&info)) {
              result.success = true;
              result.age = base::Time::Now() - info.last_modified;
              contents.CopyToString(&result.contents);
            }
          } else {
            LOG(ERROR) << "Bad checksum for cache key " << key;
          }
        }
      }
    }
    callback_runner->PostTask(FROM_HERE,
                              base::Bind(&PpdCacheImpl::ReplyAndRelease, this,
                                         base::Bind(cb, result)));
  }

  // If |cb| is non-null, invoke it on this thread, then decrement the refcount.
  void ReplyAndRelease(const base::Callback<void()>& cb) {
    if (!cb.is_null()) {
      cb.Run();
    }
    --inflight_ops_;
    Release();
    // Object may be destroyed here, so no further access to object data
    // allowed.
  }

  // Store implementation, runs on the i/o thread.
  void StoreImpl(
      const std::string& key,
      const std::string& contents,
      const scoped_refptr<base::SequencedTaskRunner>& callback_runner,
      const base::Callback<void()> cb) {
    base::ThreadRestrictions::AssertIOAllowed();
    MaybeCreateCache();
    if (contents.size() > kMaxPpdSizeBytes) {
      LOG(ERROR) << "Ignoring attempt to cache large object";
    } else {
      auto path = FilePathForKey(key);
      base::File file(path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
      std::string checksum = crypto::SHA256HashString(contents);
      if (!file.IsValid() ||
          file.WriteAtCurrentPos(contents.data(), contents.size()) !=
              static_cast<int>(contents.size()) ||
          file.WriteAtCurrentPos(checksum.data(), checksum.size()) !=
              static_cast<int>(checksum.size())) {
        LOG(ERROR) << "Failed to create ppd cache file";
        file.Close();
        if (!base::DeleteFile(path, false)) {
          LOG(ERROR) << "Failed to cleanup failed creation.";
        }
      }
    }
    callback_runner->PostTask(
        FROM_HERE, base::Bind(&PpdCacheImpl::ReplyAndRelease, this, cb));
  }

  // Return the (full) path to the file we expect to find the given key at.
  base::FilePath FilePathForKey(const std::string& key) {
    std::string hashed_key = crypto::SHA256HashString(key);
    return cache_base_dir_.Append(
        base::HexEncode(hashed_key.data(), hashed_key.size()));
  }

  int inflight_ops_ = 0;

  base::FilePath cache_base_dir_;
  scoped_refptr<base::SequencedTaskRunner> disk_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PpdCacheImpl);
};

}  // namespace

// static
scoped_refptr<PpdCache> PpdCache::Create(
    const base::FilePath& cache_base_dir,
    scoped_refptr<base::SequencedTaskRunner> disk_task_runner) {
  return scoped_refptr<PpdCache>(
      new PpdCacheImpl(cache_base_dir, disk_task_runner));
}

}  // namespace printing
}  // namespace chromeos
