// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_database.h"

#include "base/file_util.h"
#include "base/histogram.h"
#include "chrome/browser/safe_browsing/bloom_filter.h"
#include "chrome/browser/safe_browsing/safe_browsing_database_bloom.h"

using base::Time;

// Filename suffix for the bloom filter.
static const FilePath::CharType kBloomFilterFile[] =
    FILE_PATH_LITERAL(" Filter 2");

// Factory method.
SafeBrowsingDatabase* SafeBrowsingDatabase::Create() {
  return new SafeBrowsingDatabaseBloom;
}

SafeBrowsingDatabase::~SafeBrowsingDatabase() {
}

// static
FilePath SafeBrowsingDatabase::BloomFilterFilename(
    const FilePath& db_filename) {
  return FilePath(db_filename.value() + kBloomFilterFile);
}

void SafeBrowsingDatabase::LoadBloomFilter() {
  DCHECK(!bloom_filter_filename_.empty());

  // If we're missing either of the database or filter files, we wait until the
  // next update to generate a new filter.
  // TODO(paulg): Investigate how often the filter file is missing and how
  // expensive it would be to regenerate it.
  int64 size_64;
  if (!file_util::GetFileSize(filename_, &size_64) || size_64 == 0)
    return;

  if (!file_util::GetFileSize(bloom_filter_filename_, &size_64) ||
      size_64 == 0) {
    UMA_HISTOGRAM_COUNTS("SB2.FilterMissing", 1);
    return;
  }

  // We have a bloom filter file, so use that as our filter.
  Time before = Time::Now();
  bloom_filter_ = BloomFilter::LoadFile(bloom_filter_filename_);
  SB_DLOG(INFO) << "SafeBrowsingDatabase read bloom filter in "
                << (Time::Now() - before).InMilliseconds() << " ms";

  if (!bloom_filter_.get())
    UMA_HISTOGRAM_COUNTS("SB2.FilterReadFail", 1);
}

void SafeBrowsingDatabase::DeleteBloomFilter() {
  file_util::Delete(bloom_filter_filename_, false);
}

void SafeBrowsingDatabase::WriteBloomFilter() {
  if (!bloom_filter_.get())
    return;

  Time before = Time::Now();
  bool write_ok = bloom_filter_->WriteFile(bloom_filter_filename_);
  SB_DLOG(INFO) << "SafeBrowsingDatabase wrote bloom filter in " <<
      (Time::Now() - before).InMilliseconds() << " ms";

  if (!write_ok)
    UMA_HISTOGRAM_COUNTS("SB2.FilterWriteFail", 1);
}
