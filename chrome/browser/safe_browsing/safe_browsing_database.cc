// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_database.h"

#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/time.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/sha2.h"
#include "chrome/browser/safe_browsing/bloom_filter.h"
#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"
#include "chrome/browser/safe_browsing/safe_browsing_store_sqlite.h"
#include "googleurl/src/gurl.h"

namespace {

// Filename suffix for the bloom filter.
const FilePath::CharType kBloomFilterFile[] = FILE_PATH_LITERAL(" Filter 2");

// The maximum staleness for a cached entry.
const int kMaxStalenessMinutes = 45;

// To save space, the incoming |chunk_id| and |list_id| are combined
// into an |encoded_chunk_id| for storage by shifting the |list_id|
// into the low-order bits.  These functions decode that information.
int DecodeListId(const int encoded_chunk_id) {
  return encoded_chunk_id & 1;
}
int DecodeChunkId(int encoded_chunk_id) {
  return encoded_chunk_id >> 1;
}
int EncodeChunkId(int chunk, int list_id) {
  DCHECK(list_id == 0 || list_id == 1);
  return chunk << 1 | list_id;
}

// Generate the set of prefixes to check for |url|.
// TODO(shess): This function is almost the same as
// |CompareFullHashes()| in safe_browsing_util.cc, except that code
// does an early exit on match.  Since match should be the infrequent
// case (phishing or malware found), consider combining this function
// with that one.
void PrefixesToCheck(const GURL& url, std::vector<SBPrefix>* prefixes) {
  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.host());
  } else {
    safe_browsing_util::GenerateHostsToCheck(url, &hosts);
  }

  std::vector<std::string> paths;
  safe_browsing_util::GeneratePathsToCheck(url, &paths);

  for (size_t i = 0; i < hosts.size(); ++i) {
    for (size_t j = 0; j < paths.size(); ++j) {
      SBFullHash full_hash;
      base::SHA256HashString(hosts[i] + paths[j], &full_hash,
                             sizeof(SBFullHash));
      prefixes->push_back(full_hash.prefix);
    }
  }
}

// Find the entries in |full_hashes| with prefix in |prefix_hits|, and
// add them to |full_hits| if not expired.  "Not expired" is when
// either |last_update| was recent enough, or the item has been
// received recently enough.  Expired items are not deleted because a
// future update may make them acceptable again.
//
// For efficiency reasons the code walks |prefix_hits| and
// |full_hashes| in parallel, so they must be sorted by prefix.
void GetCachedFullHashes(const std::vector<SBPrefix>& prefix_hits,
                         const std::vector<SBAddFullHash>& full_hashes,
                         std::vector<SBFullHashResult>* full_hits,
                         base::Time last_update) {
  const base::Time expire_time =
      base::Time::Now() - base::TimeDelta::FromMinutes(kMaxStalenessMinutes);

  std::vector<SBPrefix>::const_iterator piter = prefix_hits.begin();
  std::vector<SBAddFullHash>::const_iterator hiter = full_hashes.begin();

  while (piter != prefix_hits.end() && hiter != full_hashes.end()) {
    if (*piter < hiter->full_hash.prefix) {
      ++piter;
    } else if (hiter->full_hash.prefix < *piter) {
      ++hiter;
    } else {
      if (expire_time < last_update ||
          expire_time.ToTimeT() < hiter->received) {
        SBFullHashResult result;
        const int list_id = DecodeListId(hiter->chunk_id);
        result.list_name = safe_browsing_util::GetListName(list_id);
        result.add_chunk_id = DecodeChunkId(hiter->chunk_id);
        result.hash = hiter->full_hash;
        full_hits->push_back(result);
      }

      // Only increment |hiter|, |piter| might have multiple hits.
      ++hiter;
    }
  }
}

// Helper for |UpdateStarted()|.  Separates |chunks| into malware and
// phishing vectors, and converts the results into range strings.
void GetChunkIds(const std::vector<int>& chunks,
                 std::string* malware_list, std::string* phishing_list) {
  std::vector<int> malware_chunks;
  std::vector<int> phishing_chunks;

  for (std::vector<int>::const_iterator iter = chunks.begin();
       iter != chunks.end(); ++iter) {
    if (safe_browsing_util::MALWARE == DecodeListId(*iter)) {
      malware_chunks.push_back(DecodeChunkId(*iter));
    } else if (safe_browsing_util::PHISH == DecodeListId(*iter)) {
      phishing_chunks.push_back(DecodeChunkId(*iter));
    } else {
      NOTREACHED();
    }
  }

  ChunksToRangeString(malware_chunks, malware_list);
  ChunksToRangeString(phishing_chunks, phishing_list);
}

// Order |SBAddFullHash| on the prefix part.  |SBAddPrefixLess()| from
// safe_browsing_store.h orders on both chunk-id and prefix.
bool SBAddFullHashPrefixLess(const SBAddFullHash& a, const SBAddFullHash& b) {
  return a.full_hash.prefix < b.full_hash.prefix;
}

}  // namespace

// Factory method.
// TODO(shess): Milestone-7 is converting from SQLite-based
// SafeBrowsingDatabaseBloom to the new file format with
// SafeBrowsingDatabaseNew.  Once that conversion is too far along to
// consider reversing, circle back and lift SafeBrowsingDatabaseNew up
// to SafeBrowsingDatabase and get rid of the abstract class.
SafeBrowsingDatabase* SafeBrowsingDatabase::Create() {
  return new SafeBrowsingDatabaseNew(new SafeBrowsingStoreFile);
}

SafeBrowsingDatabase::~SafeBrowsingDatabase() {
}

// static
FilePath SafeBrowsingDatabase::BloomFilterForFilename(
    const FilePath& db_filename) {
  return FilePath(db_filename.value() + kBloomFilterFile);
}

// static
void SafeBrowsingDatabase::RecordFailure(FailureType failure_type) {
  UMA_HISTOGRAM_ENUMERATION("SB2.DatabaseFailure", failure_type,
                            FAILURE_DATABASE_MAX);
}

SafeBrowsingDatabaseNew::SafeBrowsingDatabaseNew(SafeBrowsingStore* store)
    : creation_loop_(MessageLoop::current()),
      store_(store),
      ALLOW_THIS_IN_INITIALIZER_LIST(reset_factory_(this)),
      corruption_detected_(false) {
  DCHECK(store_.get());
}

SafeBrowsingDatabaseNew::SafeBrowsingDatabaseNew()
    : creation_loop_(MessageLoop::current()),
      store_(new SafeBrowsingStoreSqlite),
      ALLOW_THIS_IN_INITIALIZER_LIST(reset_factory_(this)) {
  DCHECK(store_.get());
}

SafeBrowsingDatabaseNew::~SafeBrowsingDatabaseNew() {
  DCHECK_EQ(creation_loop_, MessageLoop::current());
}

void SafeBrowsingDatabaseNew::Init(const FilePath& filename) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  // NOTE: There is no need to grab the lock in this function, since
  // until it returns, there are no pointers to this class on other
  // threads.  Then again, that means there is no possibility of
  // contention on the lock...
  AutoLock locked(lookup_lock_);

  DCHECK(filename_.empty());  // Ensure we haven't been run before.

  filename_ = filename;
  store_->Init(
      filename_,
      NewCallback(this, &SafeBrowsingDatabaseNew::HandleCorruptDatabase));

  full_hashes_.clear();
  pending_hashes_.clear();

  bloom_filter_filename_ = BloomFilterForFilename(filename_);
  LoadBloomFilter();
}

bool SafeBrowsingDatabaseNew::ResetDatabase() {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  // Delete files on disk.
  // TODO(shess): Hard to see where one might want to delete without a
  // reset.  Perhaps inline |Delete()|?
  if (!Delete())
    return false;

  // Reset objects in memory.
  {
    AutoLock locked(lookup_lock_);
    full_hashes_.clear();
    pending_hashes_.clear();
    prefix_miss_cache_.clear();
    // TODO(shess): This could probably be |bloom_filter_.reset()|.
    bloom_filter_ = new BloomFilter(BloomFilter::kBloomFilterMinSize *
                                    BloomFilter::kBloomFilterSizeRatio);
  }

  return true;
}

bool SafeBrowsingDatabaseNew::ContainsUrl(
    const GURL& url,
    std::string* matching_list,
    std::vector<SBPrefix>* prefix_hits,
    std::vector<SBFullHashResult>* full_hits,
    base::Time last_update) {
  // Clear the results first.
  matching_list->clear();
  prefix_hits->clear();
  full_hits->clear();

  std::vector<SBPrefix> prefixes;
  PrefixesToCheck(url, &prefixes);
  if (prefixes.empty())
    return false;

  // This function is called on the I/O thread, prevent changes to
  // bloom filter and caches.
  AutoLock locked(lookup_lock_);

  if (!bloom_filter_.get())
    return false;

  // TODO(erikkay): Not filling in matching_list - is that OK?
  size_t miss_count = 0;
  for (size_t i = 0; i < prefixes.size(); ++i) {
    if (bloom_filter_->Exists(prefixes[i])) {
      prefix_hits->push_back(prefixes[i]);
      if (prefix_miss_cache_.count(prefixes[i]) > 0)
        ++miss_count;
    }
  }

  // If all the prefixes are cached as 'misses', don't issue a GetHash.
  if (miss_count == prefix_hits->size())
    return false;

  // Find the matching full-hash results.  |full_hashes_| are from the
  // database, |pending_hashes_| are from GetHash requests between
  // updates.
  std::sort(prefix_hits->begin(), prefix_hits->end());
  GetCachedFullHashes(*prefix_hits, full_hashes_, full_hits, last_update);
  GetCachedFullHashes(*prefix_hits, pending_hashes_, full_hits, last_update);
  return true;
}

// Helper to insert entries for all of the prefixes or full hashes in
// |entry| into the store.
void SafeBrowsingDatabaseNew::InsertAdd(int chunk_id, SBPrefix host,
                                        const SBEntry* entry, int list_id) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  STATS_COUNTER("SB.HostInsert", 1);
  const int encoded_chunk_id = EncodeChunkId(chunk_id, list_id);
  const int count = entry->prefix_count();

  DCHECK(!entry->IsSub());
  if (!count) {
    // No prefixes, use host instead.
    STATS_COUNTER("SB.PrefixAdd", 1);
    store_->WriteAddPrefix(encoded_chunk_id, host);
  } else if (entry->IsPrefix()) {
    // Prefixes only.
    for (int i = 0; i < count; i++) {
      const SBPrefix prefix = entry->PrefixAt(i);
      STATS_COUNTER("SB.PrefixAdd", 1);
      store_->WriteAddPrefix(encoded_chunk_id, prefix);
    }
  } else {
    // Prefixes and hashes.
    const base::Time receive_time = base::Time::Now();
    for (int i = 0; i < count; ++i) {
      const SBFullHash full_hash = entry->FullHashAt(i);
      const SBPrefix prefix = full_hash.prefix;

      STATS_COUNTER("SB.PrefixAdd", 1);
      store_->WriteAddPrefix(encoded_chunk_id, prefix);

      STATS_COUNTER("SB.PrefixAddFull", 1);
      store_->WriteAddHash(encoded_chunk_id, receive_time, full_hash);
    }
  }
}

// Helper to iterate over all the entries in the hosts in |chunks| and
// add them to the store.
void SafeBrowsingDatabaseNew::InsertAddChunks(int list_id,
                                              const SBChunkList& chunks) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());
  for (SBChunkList::const_iterator citer = chunks.begin();
       citer != chunks.end(); ++citer) {
    const int chunk_id = citer->chunk_number;

    // The server can give us a chunk that we already have because
    // it's part of a range.  Don't add it again.
    const int encoded_chunk_id = EncodeChunkId(chunk_id, list_id);
    if (store_->CheckAddChunk(encoded_chunk_id))
      continue;

    store_->SetAddChunk(encoded_chunk_id);
    for (std::deque<SBChunkHost>::const_iterator hiter = citer->hosts.begin();
         hiter != citer->hosts.end(); ++hiter) {
      // NOTE: Could pass |encoded_chunk_id|, but then inserting add
      // chunks would look different from inserting sub chunks.
      InsertAdd(chunk_id, hiter->host, hiter->entry, list_id);
    }
  }
}

// Helper to insert entries for all of the prefixes or full hashes in
// |entry| into the store.
void SafeBrowsingDatabaseNew::InsertSub(int chunk_id, SBPrefix host,
                                        const SBEntry* entry, int list_id) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  STATS_COUNTER("SB.HostDelete", 1);
  const int encoded_chunk_id = EncodeChunkId(chunk_id, list_id);
  const int count = entry->prefix_count();

  DCHECK(entry->IsSub());
  if (!count) {
    // No prefixes, use host instead.
    STATS_COUNTER("SB.PrefixSub", 1);
    const int add_chunk_id = EncodeChunkId(entry->chunk_id(), list_id);
    store_->WriteSubPrefix(encoded_chunk_id, add_chunk_id, host);
  } else if (entry->IsPrefix()) {
    // Prefixes only.
    for (int i = 0; i < count; i++) {
      const SBPrefix prefix = entry->PrefixAt(i);
      const int add_chunk_id =
          EncodeChunkId(entry->ChunkIdAtPrefix(i), list_id);

      STATS_COUNTER("SB.PrefixSub", 1);
      store_->WriteSubPrefix(encoded_chunk_id, add_chunk_id, prefix);
    }
  } else {
    // Prefixes and hashes.
    for (int i = 0; i < count; ++i) {
      const SBFullHash full_hash = entry->FullHashAt(i);
      const int add_chunk_id =
          EncodeChunkId(entry->ChunkIdAtPrefix(i), list_id);

      STATS_COUNTER("SB.PrefixSub", 1);
      store_->WriteSubPrefix(encoded_chunk_id, add_chunk_id, full_hash.prefix);

      STATS_COUNTER("SB.PrefixSubFull", 1);
      store_->WriteSubHash(encoded_chunk_id, add_chunk_id, full_hash);
    }
  }
}

// Helper to iterate over all the entries in the hosts in |chunks| and
// add them to the store.
void SafeBrowsingDatabaseNew::InsertSubChunks(int list_id,
                                              const SBChunkList& chunks) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());
  for (SBChunkList::const_iterator citer = chunks.begin();
       citer != chunks.end(); ++citer) {
    const int chunk_id = citer->chunk_number;

    // The server can give us a chunk that we already have because
    // it's part of a range.  Don't add it again.
    const int encoded_chunk_id = EncodeChunkId(chunk_id, list_id);
    if (store_->CheckSubChunk(encoded_chunk_id))
      continue;

    store_->SetSubChunk(encoded_chunk_id);
    for (std::deque<SBChunkHost>::const_iterator hiter = citer->hosts.begin();
         hiter != citer->hosts.end(); ++hiter) {
      InsertSub(chunk_id, hiter->host, hiter->entry, list_id);
    }
  }
}

void SafeBrowsingDatabaseNew::InsertChunks(const std::string& list_name,
                                           const SBChunkList& chunks) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  if (corruption_detected_ || chunks.empty())
    return;

  const base::Time insert_start = base::Time::Now();

  const int list_id = safe_browsing_util::GetListId(list_name);
  store_->BeginChunk();
  if (chunks.front().is_add) {
    InsertAddChunks(list_id, chunks);
  } else {
    InsertSubChunks(list_id, chunks);
  }
  store_->FinishChunk();

  UMA_HISTOGRAM_TIMES("SB2.ChunkInsert", base::Time::Now() - insert_start);
}

void SafeBrowsingDatabaseNew::DeleteChunks(
    const std::vector<SBChunkDelete>& chunk_deletes) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  if (corruption_detected_ || chunk_deletes.empty())
    return;

  const std::string& list_name = chunk_deletes.front().list_name;
  const int list_id = safe_browsing_util::GetListId(list_name);

  for (size_t i = 0; i < chunk_deletes.size(); ++i) {
    std::vector<int> chunk_numbers;
    RangesToChunks(chunk_deletes[i].chunk_del, &chunk_numbers);
    for (size_t j = 0; j < chunk_numbers.size(); ++j) {
      const int encoded_chunk_id = EncodeChunkId(chunk_numbers[j], list_id);
      if (chunk_deletes[i].is_sub_del)
        store_->DeleteSubChunk(encoded_chunk_id);
      else
        store_->DeleteAddChunk(encoded_chunk_id);
    }
  }
}

void SafeBrowsingDatabaseNew::CacheHashResults(
    const std::vector<SBPrefix>& prefixes,
    const std::vector<SBFullHashResult>& full_hits) {
  // This is called on the I/O thread, lock against updates.
  AutoLock locked(lookup_lock_);

  if (full_hits.empty()) {
    prefix_miss_cache_.insert(prefixes.begin(), prefixes.end());
    return;
  }

  // TODO(shess): SBFullHashResult and SBAddFullHash are very similar.
  // Refactor to make them identical.
  const base::Time now = base::Time::Now();
  const size_t orig_size = pending_hashes_.size();
  for (std::vector<SBFullHashResult>::const_iterator iter = full_hits.begin();
       iter != full_hits.end(); ++iter) {
    const int list_id = safe_browsing_util::GetListId(iter->list_name);
    const int encoded_chunk_id = EncodeChunkId(iter->add_chunk_id, list_id);
    pending_hashes_.push_back(SBAddFullHash(encoded_chunk_id, now, iter->hash));
  }

  // Sort new entries then merge with the previously-sorted entries.
  std::vector<SBAddFullHash>::iterator
      orig_end = pending_hashes_.begin() + orig_size;
  std::sort(orig_end, pending_hashes_.end(), SBAddFullHashPrefixLess);
  std::inplace_merge(pending_hashes_.begin(), orig_end, pending_hashes_.end(),
                     SBAddFullHashPrefixLess);
}

bool SafeBrowsingDatabaseNew::UpdateStarted(
    std::vector<SBListChunkRanges>* lists) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());
  DCHECK(lists);

  // If |BeginUpdate()| fails, reset the database.
  if (!store_->BeginUpdate()) {
    RecordFailure(FAILURE_DATABASE_UPDATE_BEGIN);
    HandleCorruptDatabase();
    return false;
  }

  SBListChunkRanges malware(safe_browsing_util::kMalwareList);
  SBListChunkRanges phishing(safe_browsing_util::kPhishingList);

  std::vector<int> add_chunks;
  store_->GetAddChunks(&add_chunks);
  GetChunkIds(add_chunks, &malware.adds, &phishing.adds);

  std::vector<int> sub_chunks;
  store_->GetSubChunks(&sub_chunks);
  GetChunkIds(sub_chunks, &malware.subs, &phishing.subs);

  lists->push_back(malware);
  lists->push_back(phishing);

  corruption_detected_ = false;

  return true;
}

void SafeBrowsingDatabaseNew::UpdateFinished(bool update_succeeded) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  if (corruption_detected_)
    return;

  // Unroll any partially-received transaction.
  if (!update_succeeded) {
    store_->CancelUpdate();
    return;
  }

  // Copy out the pending add hashes.  Copy rather than swapping in
  // case |ContainsURL()| is called before the new filter is complete.
  std::vector<SBAddFullHash> pending_add_hashes;
  {
    AutoLock locked(lookup_lock_);
    pending_add_hashes.insert(pending_add_hashes.end(),
                              pending_hashes_.begin(), pending_hashes_.end());
  }

  // Measure the amount of IO during the bloom filter build.
  base::IoCounters io_before, io_after;
  base::ProcessHandle handle = base::Process::Current().handle();
  scoped_ptr<base::ProcessMetrics> metric(
#if !defined(OS_MACOSX)
      base::ProcessMetrics::CreateProcessMetrics(handle)
#else
      // Getting stats only for the current process is enough, so NULL is fine.
      base::ProcessMetrics::CreateProcessMetrics(handle, NULL)
#endif
  );

  // IoCounters are currently not supported on Mac, and may not be
  // available for Linux, so we check the result and only show IO
  // stats if they are available.
  const bool got_counters = metric->GetIOCounters(&io_before);

  const base::Time before = base::Time::Now();

  std::vector<SBAddPrefix> add_prefixes;
  std::vector<SBAddFullHash> add_full_hashes;
  if (!store_->FinishUpdate(pending_add_hashes,
                            &add_prefixes, &add_full_hashes)) {
    RecordFailure(FAILURE_DATABASE_UPDATE_FINISH);
    return;
  }

  // Create and populate |filter| from |add_prefixes|.
  // TODO(shess): The bloom filter doesn't need to be a
  // scoped_refptr<> for this code.  Refactor that away.
  const int filter_size =
      BloomFilter::FilterSizeForKeyCount(add_prefixes.size());
  scoped_refptr<BloomFilter> filter(new BloomFilter(filter_size));
  for (size_t i = 0; i < add_prefixes.size(); ++i) {
    filter->Insert(add_prefixes[i].prefix);
  }

  // This needs to be in sorted order by prefix for efficient access.
  std::sort(add_full_hashes.begin(), add_full_hashes.end(),
            SBAddFullHashPrefixLess);

  // Swap in the newly built filter and cache.
  {
    AutoLock locked(lookup_lock_);
    full_hashes_.swap(add_full_hashes);

    // TODO(shess): If |CacheHashResults()| is posted between the
    // earlier lock and this clear, those pending hashes will be lost.
    // It could be fixed by only removing hashes which were collected
    // at the earlier point.  I believe that is fail-safe as-is (the
    // hash will be fetched again).
    pending_hashes_.clear();

    prefix_miss_cache_.clear();
    bloom_filter_.swap(filter);
  }

  const base::TimeDelta bloom_gen = base::Time::Now() - before;

  // Persist the bloom filter to disk.  Since only this thread changes
  // |bloom_filter_|, there is no need to lock.
  WriteBloomFilter();

  // Gather statistics.
  if (got_counters && metric->GetIOCounters(&io_after)) {
    UMA_HISTOGRAM_COUNTS("SB2.BuildReadKilobytes",
                         static_cast<int>(io_after.ReadTransferCount -
                                          io_before.ReadTransferCount) / 1024);
    UMA_HISTOGRAM_COUNTS("SB2.BuildWriteKilobytes",
                         static_cast<int>(io_after.WriteTransferCount -
                                          io_before.WriteTransferCount) / 1024);
    UMA_HISTOGRAM_COUNTS("SB2.BuildReadOperations",
                         static_cast<int>(io_after.ReadOperationCount -
                                          io_before.ReadOperationCount));
    UMA_HISTOGRAM_COUNTS("SB2.BuildWriteOperations",
                         static_cast<int>(io_after.WriteOperationCount -
                                          io_before.WriteOperationCount));
  }
  VLOG(1) << "SafeBrowsingDatabaseImpl built bloom filter in "
          << bloom_gen.InMilliseconds() << " ms total.  prefix count: "
          << add_prefixes.size();
  UMA_HISTOGRAM_LONG_TIMES("SB2.BuildFilter", bloom_gen);
  UMA_HISTOGRAM_COUNTS("SB2.FilterKilobytes", bloom_filter_->size() / 1024);
  int64 size_64;
  if (file_util::GetFileSize(filename_, &size_64)) {
    UMA_HISTOGRAM_COUNTS("SB2.DatabaseKilobytes",
                         static_cast<int>(size_64 / 1024));
  }
}

void SafeBrowsingDatabaseNew::HandleCorruptDatabase() {
  // Reset the database after the current task has unwound (but only
  // reset once within the scope of a given task).
  if (reset_factory_.empty()) {
    RecordFailure(FAILURE_DATABASE_CORRUPT);
    MessageLoop::current()->PostTask(FROM_HERE,
        reset_factory_.NewRunnableMethod(
            &SafeBrowsingDatabaseNew::OnHandleCorruptDatabase));
  }
}

void SafeBrowsingDatabaseNew::OnHandleCorruptDatabase() {
  UMA_HISTOGRAM_COUNTS("SB2.HandleCorrupt", 1);
  RecordFailure(FAILURE_DATABASE_CORRUPT_HANDLER);
  corruption_detected_ = true;  // Stop updating the database.
  ResetDatabase();
  DCHECK(false) << "SafeBrowsing database was corrupt and reset";
}

// TODO(shess): I'm not clear why this code doesn't have any
// real error-handling.
void SafeBrowsingDatabaseNew::LoadBloomFilter() {
  DCHECK_EQ(creation_loop_, MessageLoop::current());
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
    RecordFailure(FAILURE_DATABASE_FILTER_MISSING);
    return;
  }

  const base::TimeTicks before = base::TimeTicks::Now();
  bloom_filter_ = BloomFilter::LoadFile(bloom_filter_filename_);
  VLOG(1) << "SafeBrowsingDatabaseNew read bloom filter in "
          << (base::TimeTicks::Now() - before).InMilliseconds() << " ms";

  if (!bloom_filter_.get()) {
    UMA_HISTOGRAM_COUNTS("SB2.FilterReadFail", 1);
    RecordFailure(FAILURE_DATABASE_FILTER_READ);
  }
}

bool SafeBrowsingDatabaseNew::Delete() {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  const bool r1 = store_->Delete();
  if (!r1)
    RecordFailure(FAILURE_DATABASE_STORE_DELETE);
  const bool r2 = file_util::Delete(bloom_filter_filename_, false);
  if (!r2)
    RecordFailure(FAILURE_DATABASE_FILTER_DELETE);
  return r1 && r2;
}

void SafeBrowsingDatabaseNew::WriteBloomFilter() {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  if (!bloom_filter_.get())
    return;

  const base::TimeTicks before = base::TimeTicks::Now();
  const bool write_ok = bloom_filter_->WriteFile(bloom_filter_filename_);
  VLOG(1) << "SafeBrowsingDatabaseNew wrote bloom filter in "
          << (base::TimeTicks::Now() - before).InMilliseconds() << " ms";

  if (!write_ok) {
    UMA_HISTOGRAM_COUNTS("SB2.FilterWriteFail", 1);
    RecordFailure(FAILURE_DATABASE_FILTER_WRITE);
  }
}
