// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_database.h"

#include <algorithm>
#include <iterator>

#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/time.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "crypto/sha2.h"
#include "chrome/browser/safe_browsing/bloom_filter.h"
#include "chrome/browser/safe_browsing/prefix_set.h"
#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

namespace {

// Filename suffix for the bloom filter.
const FilePath::CharType kBloomFilterFile[] = FILE_PATH_LITERAL(" Filter 2");
// Filename suffix for download store.
const FilePath::CharType kDownloadDBFile[] = FILE_PATH_LITERAL(" Download");
// Filename suffix for client-side phishing detection whitelist store.
const FilePath::CharType kCsdWhitelistDBFile[] =
    FILE_PATH_LITERAL(" Csd Whitelist");
// Filename suffix for browse store.
// TODO(lzheng): change to a better name when we change the file format.
const FilePath::CharType kBrowseDBFile[] = FILE_PATH_LITERAL(" Bloom");

// The maximum staleness for a cached entry.
const int kMaxStalenessMinutes = 45;

// Maximum number of entries we allow in the client-side phishing detection
// whitelist.  If the whitelist on disk contains more entries then
// ContainsCsdWhitelistedUrl will always return true.
const size_t kMaxCsdWhitelistSize = 5000;

// If the hash of this exact expression is on the csd whitelist then
// ContainsCsdWhitelistedUrl will always return true.
const char kCsdKillSwitchUrl[] =
    "sb-ssl.google.com/safebrowsing/csd/killswitch";

// To save space, the incoming |chunk_id| and |list_id| are combined
// into an |encoded_chunk_id| for storage by shifting the |list_id|
// into the low-order bits.  These functions decode that information.
// TODO(lzheng): It was reasonable when database is saved in sqlite, but
// there should be better ways to save chunk_id and list_id after we use
// SafeBrowsingStoreFile.
int GetListIdBit(const int encoded_chunk_id) {
  return encoded_chunk_id & 1;
}
int DecodeChunkId(int encoded_chunk_id) {
  return encoded_chunk_id >> 1;
}
int EncodeChunkId(const int chunk, const int list_id) {
  DCHECK_NE(list_id, safe_browsing_util::INVALID);
  return chunk << 1 | list_id % 2;
}

// Generate the set of full hashes to check for |url|.  If
// |include_whitelist_hashes| is true we will generate additional path-prefixes
// to match against the csd whitelist.  E.g., if the path-prefix /foo is on the
// whitelist it should also match /foo/bar which is not the case for all the
// other lists.
// TODO(shess): This function is almost the same as
// |CompareFullHashes()| in safe_browsing_util.cc, except that code
// does an early exit on match.  Since match should be the infrequent
// case (phishing or malware found), consider combining this function
// with that one.
void BrowseFullHashesToCheck(const GURL& url,
                             bool include_whitelist_hashes,
                             std::vector<SBFullHash>* full_hashes) {
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
      const std::string& path = paths[j];
      SBFullHash full_hash;
      crypto::SHA256HashString(hosts[i] + path, &full_hash,
                               sizeof(full_hash));
      full_hashes->push_back(full_hash);

      // We may have /foo as path-prefix in the whitelist which should
      // also match with /foo/bar and /foo?bar.  Hence, for every path
      // that ends in '/' we also add the path without the slash.
      if (include_whitelist_hashes &&
          path.size() > 1 &&
          path[path.size() - 1] == '/') {
        crypto::SHA256HashString(hosts[i] + path.substr(0, path.size() - 1),
                                 &full_hash, sizeof(full_hash));
        full_hashes->push_back(full_hash);
      }
    }
  }
}

// Get the prefixes matching the download |urls|.
void GetDownloadUrlPrefixes(const std::vector<GURL>& urls,
                            std::vector<SBPrefix>* prefixes) {
  std::vector<SBFullHash> full_hashes;
  for (size_t i = 0; i < urls.size(); ++i)
    BrowseFullHashesToCheck(urls[i], false, &full_hashes);

  for (size_t i = 0; i < full_hashes.size(); ++i)
    prefixes->push_back(full_hashes[i].prefix);
}

// Find the entries in |full_hashes| with prefix in |prefix_hits|, and
// add them to |full_hits| if not expired.  "Not expired" is when
// either |last_update| was recent enough, or the item has been
// received recently enough.  Expired items are not deleted because a
// future update may make them acceptable again.
//
// For efficiency reasons the code walks |prefix_hits| and
// |full_hashes| in parallel, so they must be sorted by prefix.
void GetCachedFullHashesForBrowse(const std::vector<SBPrefix>& prefix_hits,
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
        const int list_bit = GetListIdBit(hiter->chunk_id);
        DCHECK(list_bit == safe_browsing_util::MALWARE ||
               list_bit == safe_browsing_util::PHISH);
        if (!safe_browsing_util::GetListName(list_bit, &result.list_name))
          continue;
        result.add_chunk_id = DecodeChunkId(hiter->chunk_id);
        result.hash = hiter->full_hash;
        full_hits->push_back(result);
      }

      // Only increment |hiter|, |piter| might have multiple hits.
      ++hiter;
    }
  }
}

// This function generates a chunk range string for |chunks|. It
// outputs one chunk range string per list and writes it to the
// |list_ranges| vector.  We expect |list_ranges| to already be of the
// right size.  E.g., if |chunks| contains chunks with two different
// list ids then |list_ranges| must contain two elements.
void GetChunkRanges(const std::vector<int>& chunks,
                    std::vector<std::string>* list_ranges) {
  DCHECK_GT(list_ranges->size(), 0U);
  DCHECK_LE(list_ranges->size(), 2U);
  std::vector<std::vector<int> > decoded_chunks(list_ranges->size());
  for (std::vector<int>::const_iterator iter = chunks.begin();
       iter != chunks.end(); ++iter) {
    int mod_list_id = GetListIdBit(*iter);
    DCHECK_GE(mod_list_id, 0);
    DCHECK_LT(static_cast<size_t>(mod_list_id), decoded_chunks.size());
    decoded_chunks[mod_list_id].push_back(DecodeChunkId(*iter));
  }
  for (size_t i = 0; i < decoded_chunks.size(); ++i) {
    ChunksToRangeString(decoded_chunks[i], &((*list_ranges)[i]));
  }
}

// Helper function to create chunk range lists for Browse related
// lists.
void UpdateChunkRanges(SafeBrowsingStore* store,
                       const std::vector<std::string>& listnames,
                       std::vector<SBListChunkRanges>* lists) {
  DCHECK_GT(listnames.size(), 0U);
  DCHECK_LE(listnames.size(), 2U);
  std::vector<int> add_chunks;
  std::vector<int> sub_chunks;
  store->GetAddChunks(&add_chunks);
  store->GetSubChunks(&sub_chunks);

  std::vector<std::string> adds(listnames.size());
  std::vector<std::string> subs(listnames.size());
  GetChunkRanges(add_chunks, &adds);
  GetChunkRanges(sub_chunks, &subs);

  for (size_t i = 0; i < listnames.size(); ++i) {
    const std::string& listname = listnames[i];
    DCHECK_EQ(safe_browsing_util::GetListId(listname) % 2,
              static_cast<int>(i % 2));
    DCHECK_NE(safe_browsing_util::GetListId(listname),
              safe_browsing_util::INVALID);
    lists->push_back(SBListChunkRanges(listname));
    lists->back().adds.swap(adds[i]);
    lists->back().subs.swap(subs[i]);
  }
}

// Order |SBAddFullHash| on the prefix part.  |SBAddPrefixLess()| from
// safe_browsing_store.h orders on both chunk-id and prefix.
bool SBAddFullHashPrefixLess(const SBAddFullHash& a, const SBAddFullHash& b) {
  return a.full_hash.prefix < b.full_hash.prefix;
}

// As compared to the bloom filter, PrefixSet should have these
// properties:
// - Any bloom filter miss should be a prefix set miss.
// - Any prefix set hit should be a bloom filter hit.
// - Bloom filter false positives are prefix set misses.
// The following is to log actual performance to verify this.
enum PrefixSetEvent {
  PREFIX_SET_EVENT_HIT,
  PREFIX_SET_EVENT_BLOOM_HIT,
  PREFIX_SET_EVENT_BLOOM_MISS_PREFIX_HIT,
  PREFIX_SET_EVENT_BLOOM_MISS_PREFIX_HIT_INVALID,
  PREFIX_SET_GETPREFIXES_BROKEN,
  PREFIX_SET_GETPREFIXES_BROKEN_SIZE,
  PREFIX_SET_GETPREFIXES_FIRST_BROKEN,
  PREFIX_SET_SBPREFIX_WAS_BROKEN,
  PREFIX_SET_GETPREFIXES_BROKEN_SORTING,
  PREFIX_SET_GETPREFIXES_BROKEN_DUPLICATION,
  PREFIX_SET_GETPREFIX_UNSORTED_IS_DELTA,
  PREFIX_SET_GETPREFIX_UNSORTED_IS_INDEX,
  PREFIX_SET_GETPREFIX_CHECKSUM_MISMATCH,

  // Memory space for histograms is determined by the max.  ALWAYS ADD
  // NEW VALUES BEFORE THIS ONE.
  PREFIX_SET_EVENT_MAX
};

void RecordPrefixSetInfo(PrefixSetEvent event_type) {
  UMA_HISTOGRAM_ENUMERATION("SB2.PrefixSetEvent", event_type,
                            PREFIX_SET_EVENT_MAX);
}

// Generate a |PrefixSet| instance from the contents of
// |add_prefixes|.  Additionally performs various checks to make sure
// that the resulting prefix set is valid, so that the
// PREFIX_SET_EVENT_BLOOM_MISS_PREFIX_HIT_INVALID histogram in
// ContainsBrowseUrl() can be trustworthy.
safe_browsing::PrefixSet* PrefixSetFromAddPrefixes(
    const std::vector<SBAddPrefix>& add_prefixes) {
  // TODO(shess): If |add_prefixes| were sorted by the prefix, it
  // could be passed directly to |PrefixSet()|, removing the need for
  // |prefixes|.  For now, |prefixes| is useful while debugging
  // things.
  std::vector<SBPrefix> prefixes;
  for (size_t i = 0; i < add_prefixes.size(); ++i) {
    prefixes.push_back(add_prefixes[i].prefix);
  }

  std::sort(prefixes.begin(), prefixes.end());
  prefixes.erase(std::unique(prefixes.begin(), prefixes.end()),
                 prefixes.end());

  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(new safe_browsing::PrefixSet(prefixes));

  std::vector<SBPrefix> restored;
  prefix_set->GetPrefixes(&restored);

  // Expect them to be equal.
  if (restored.size() == prefixes.size() &&
      std::equal(prefixes.begin(), prefixes.end(), restored.begin()))
    return prefix_set.release();

  // Log BROKEN for continuity with previous release, and SIZE to
  // distinguish which test failed.
  NOTREACHED();
  RecordPrefixSetInfo(PREFIX_SET_GETPREFIXES_BROKEN);
  if (restored.size() != prefixes.size())
    RecordPrefixSetInfo(PREFIX_SET_GETPREFIXES_BROKEN_SIZE);

  // Try to distinguish between updates from one broken user and a
  // distributed problem.
  static bool logged_broken = false;
  if (!logged_broken) {
    RecordPrefixSetInfo(PREFIX_SET_GETPREFIXES_FIRST_BROKEN);
    logged_broken = true;
  }

  // This seems so very very unlikely.  But if it ever were true, then
  // it could explain why GetPrefixes() seemed broken.
  if (sizeof(int) != sizeof(int32))
    RecordPrefixSetInfo(PREFIX_SET_SBPREFIX_WAS_BROKEN);

  // Check if memory was corrupted during construction.
  if (!prefix_set->CheckChecksum())
    RecordPrefixSetInfo(PREFIX_SET_GETPREFIX_CHECKSUM_MISMATCH);

  // Check whether |restored| is unsorted, or has duplication.
  if (restored.size()) {
    size_t unsorted_count = 0;
    bool duplicates = false;
    SBPrefix prev = restored[0];
    for (size_t i = 0; i < restored.size(); prev = restored[i], ++i) {
      if (prev > restored[i]) {
        unsorted_count++;
        UMA_HISTOGRAM_COUNTS("SB2.PrefixSetUnsortedDifference",
                             prev - restored[i]);

        // When unsorted, how big is the set, and how far are we into
        // it.  If the set is very small or large, that might inform
        // pursuit of a degenerate case.  If the percentage is close
        // to 0%, 100%, or 50%, then there might be an interesting
        // degenerate case to explore.
        UMA_HISTOGRAM_COUNTS("SB2.PrefixSetUnsortedSize", restored.size());
        UMA_HISTOGRAM_PERCENTAGE("SB2.PrefixSetUnsortedPercent",
                                 i * 100 / restored.size());

        if (prefix_set->IsDeltaAt(i)) {
          RecordPrefixSetInfo(PREFIX_SET_GETPREFIX_UNSORTED_IS_DELTA);

          // Histograms require memory on the order of the number of
          // buckets, making high-precision logging expensive.  For
          // now aim for a sense of the range of the problem.
          UMA_HISTOGRAM_CUSTOM_COUNTS("SB2.PrefixSetUnsortedDelta",
                                      prefix_set->DeltaAt(i), 1, 0xFFFF, 50);
        } else {
          RecordPrefixSetInfo(PREFIX_SET_GETPREFIX_UNSORTED_IS_INDEX);
        }
      }
      if (prev == restored[i])
        duplicates = true;
    }

    // Record findings.
    if (unsorted_count) {
      RecordPrefixSetInfo(PREFIX_SET_GETPREFIXES_BROKEN_SORTING);
      UMA_HISTOGRAM_COUNTS_100("SB2.PrefixSetUnsorted", unsorted_count);
    }
    if (duplicates)
      RecordPrefixSetInfo(PREFIX_SET_GETPREFIXES_BROKEN_DUPLICATION);

    // Fix the problems noted.  If |restored| was unsorted, then
    // |duplicates| may give a false negative.
    if (unsorted_count)
      std::sort(restored.begin(), restored.end());
    if (unsorted_count || duplicates)
      restored.erase(std::unique(restored.begin(), restored.end()),
                     restored.end());
  }

  // NOTE(shess): The following could be done using a single
  // uber-loop, but it's complicated by needing multiple parallel
  // iterators.  Didn't seem worthwhile for something that will only
  // live for a short period and only fires for one in a million
  // updates.

  // Find elements in |restored| which are not in |prefixes|.
  std::vector<SBPrefix> difference;
  std::set_difference(restored.begin(), restored.end(),
                      prefixes.begin(), prefixes.end(),
                      std::back_inserter(difference));
  if (difference.size())
    UMA_HISTOGRAM_COUNTS_100("SB2.PrefixSetRestoredExcess", difference.size());

  // Find elements in |prefixes| which are not in |restored|.
  difference.clear();
  std::set_difference(prefixes.begin(), prefixes.end(),
                      restored.begin(), restored.end(),
                      std::back_inserter(difference));
  if (difference.size())
    UMA_HISTOGRAM_COUNTS_100("SB2.PrefixSetRestoredShortfall",
                             difference.size());

  return prefix_set.release();
}

}  // namespace

// The default SafeBrowsingDatabaseFactory.
class SafeBrowsingDatabaseFactoryImpl : public SafeBrowsingDatabaseFactory {
 public:
  virtual SafeBrowsingDatabase* CreateSafeBrowsingDatabase(
      bool enable_download_protection,
      bool enable_client_side_whitelist) {
    return new SafeBrowsingDatabaseNew(
        new SafeBrowsingStoreFile,
        enable_download_protection ? new SafeBrowsingStoreFile : NULL,
        enable_client_side_whitelist ? new SafeBrowsingStoreFile : NULL);
  }

  SafeBrowsingDatabaseFactoryImpl() { }

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDatabaseFactoryImpl);
};

// static
SafeBrowsingDatabaseFactory* SafeBrowsingDatabase::factory_ = NULL;

// Factory method, non-thread safe. Caller has to make sure this s called
// on SafeBrowsing Thread.
// TODO(shess): There's no need for a factory any longer.  Convert
// SafeBrowsingDatabaseNew to SafeBrowsingDatabase, and have Create()
// callers just construct things directly.
SafeBrowsingDatabase* SafeBrowsingDatabase::Create(
    bool enable_download_protection,
    bool enable_client_side_whitelist) {
  if (!factory_)
    factory_ = new SafeBrowsingDatabaseFactoryImpl();
  return factory_->CreateSafeBrowsingDatabase(enable_download_protection,
                                              enable_client_side_whitelist);
}

SafeBrowsingDatabase::~SafeBrowsingDatabase() {
}

// static
FilePath SafeBrowsingDatabase::BrowseDBFilename(
         const FilePath& db_base_filename) {
  return FilePath(db_base_filename.value() + kBrowseDBFile);
}

// static
FilePath SafeBrowsingDatabase::DownloadDBFilename(
    const FilePath& db_base_filename) {
  return FilePath(db_base_filename.value() + kDownloadDBFile);
}

// static
FilePath SafeBrowsingDatabase::BloomFilterForFilename(
    const FilePath& db_filename) {
  return FilePath(db_filename.value() + kBloomFilterFile);
}

// static
FilePath SafeBrowsingDatabase::CsdWhitelistDBFilename(
    const FilePath& db_filename) {
  return FilePath(db_filename.value() + kCsdWhitelistDBFile);
}

SafeBrowsingStore* SafeBrowsingDatabaseNew::GetStore(const int list_id) {
  DVLOG(3) << "Get store for list: " << list_id;
  if (list_id == safe_browsing_util::PHISH ||
      list_id == safe_browsing_util::MALWARE) {
    return browse_store_.get();
  } else if (list_id == safe_browsing_util::BINURL ||
             list_id == safe_browsing_util::BINHASH) {
    return download_store_.get();
  } else if (list_id == safe_browsing_util::CSDWHITELIST) {
    return csd_whitelist_store_.get();
  }
  return NULL;
}

// static
void SafeBrowsingDatabase::RecordFailure(FailureType failure_type) {
  UMA_HISTOGRAM_ENUMERATION("SB2.DatabaseFailure", failure_type,
                            FAILURE_DATABASE_MAX);
}

SafeBrowsingDatabaseNew::SafeBrowsingDatabaseNew()
    : creation_loop_(MessageLoop::current()),
      browse_store_(new SafeBrowsingStoreFile),
      download_store_(NULL),
      csd_whitelist_store_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(reset_factory_(this)) {
  DCHECK(browse_store_.get());
  DCHECK(!download_store_.get());
  DCHECK(!csd_whitelist_store_.get());
}

SafeBrowsingDatabaseNew::SafeBrowsingDatabaseNew(
    SafeBrowsingStore* browse_store,
    SafeBrowsingStore* download_store,
    SafeBrowsingStore* csd_whitelist_store)
    : creation_loop_(MessageLoop::current()),
      browse_store_(browse_store),
      download_store_(download_store),
      csd_whitelist_store_(csd_whitelist_store),
      ALLOW_THIS_IN_INITIALIZER_LIST(reset_factory_(this)),
      corruption_detected_(false) {
  DCHECK(browse_store_.get());
}

SafeBrowsingDatabaseNew::~SafeBrowsingDatabaseNew() {
  DCHECK_EQ(creation_loop_, MessageLoop::current());
}

void SafeBrowsingDatabaseNew::Init(const FilePath& filename_base) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());
  // Ensure we haven't been run before.
  DCHECK(browse_filename_.empty());
  DCHECK(download_filename_.empty());
  DCHECK(csd_whitelist_filename_.empty());

  browse_filename_ = BrowseDBFilename(filename_base);
  bloom_filter_filename_ = BloomFilterForFilename(browse_filename_);

  browse_store_->Init(
      browse_filename_,
      NewCallback(this, &SafeBrowsingDatabaseNew::HandleCorruptDatabase));
  DVLOG(1) << "Init browse store: " << browse_filename_.value();

  {
    // NOTE: There is no need to grab the lock in this function, since
    // until it returns, there are no pointers to this class on other
    // threads.  Then again, that means there is no possibility of
    // contention on the lock...
    base::AutoLock locked(lookup_lock_);
    full_browse_hashes_.clear();
    pending_browse_hashes_.clear();
    LoadBloomFilter();
  }

  if (download_store_.get()) {
    download_filename_ = DownloadDBFilename(filename_base);
    download_store_->Init(
        download_filename_,
        NewCallback(this, &SafeBrowsingDatabaseNew::HandleCorruptDatabase));
    DVLOG(1) << "Init download store: " << download_filename_.value();
  }

  if (csd_whitelist_store_.get()) {
    csd_whitelist_filename_ = CsdWhitelistDBFilename(filename_base);
    csd_whitelist_store_->Init(
        csd_whitelist_filename_,
        NewCallback(this, &SafeBrowsingDatabaseNew::HandleCorruptDatabase));
    DVLOG(1) << "Init csd whitelist store: " << csd_whitelist_filename_.value();
    std::vector<SBAddFullHash> full_hashes;
    if (csd_whitelist_store_->GetAddFullHashes(&full_hashes)) {
      LoadCsdWhitelist(full_hashes);
    } else {
      CsdWhitelistAllUrls();
    }
  } else {
    CsdWhitelistAllUrls();  // Just to be safe.
  }
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
    base::AutoLock locked(lookup_lock_);
    full_browse_hashes_.clear();
    pending_browse_hashes_.clear();
    prefix_miss_cache_.clear();
    // TODO(shess): This could probably be |bloom_filter_.reset()|.
    browse_bloom_filter_ = new BloomFilter(BloomFilter::kBloomFilterMinSize *
                                           BloomFilter::kBloomFilterSizeRatio);
    // TODO(shess): It is simpler for the code to assume that presence
    // of a bloom filter always implies presence of a prefix set.
    prefix_set_.reset(new safe_browsing::PrefixSet(std::vector<SBPrefix>()));
  }
  // Wants to acquire the lock itself.
  CsdWhitelistAllUrls();

  return true;
}

// TODO(lzheng): Remove matching_list, it is not used anywhere.
bool SafeBrowsingDatabaseNew::ContainsBrowseUrl(
    const GURL& url,
    std::string* matching_list,
    std::vector<SBPrefix>* prefix_hits,
    std::vector<SBFullHashResult>* full_hits,
    base::Time last_update) {
  // Clear the results first.
  matching_list->clear();
  prefix_hits->clear();
  full_hits->clear();

  std::vector<SBFullHash> full_hashes;
  BrowseFullHashesToCheck(url, false, &full_hashes);
  if (full_hashes.empty())
    return false;

  // This function is called on the I/O thread, prevent changes to
  // bloom filter and caches.
  base::AutoLock locked(lookup_lock_);

  if (!browse_bloom_filter_.get())
    return false;
  DCHECK(prefix_set_.get());

  // Used to double-check in case of a hit mis-match.
  std::vector<SBPrefix> restored;

  size_t miss_count = 0;
  for (size_t i = 0; i < full_hashes.size(); ++i) {
    bool found = prefix_set_->Exists(full_hashes[i].prefix);

    if (browse_bloom_filter_->Exists(full_hashes[i].prefix)) {
      RecordPrefixSetInfo(PREFIX_SET_EVENT_BLOOM_HIT);
      if (found)
        RecordPrefixSetInfo(PREFIX_SET_EVENT_HIT);
      prefix_hits->push_back(full_hashes[i].prefix);
      if (prefix_miss_cache_.count(full_hashes[i].prefix) > 0)
        ++miss_count;
    } else {
      // Bloom filter misses should never be in prefix set.  Re-create
      // the original prefixes and manually search for it, to check if
      // there's a bug with how |Exists()| is implemented.
      // |UpdateBrowseStore()| previously verified that
      // |GetPrefixes()| returns the same prefixes as were passed to
      // the constructor.
      DCHECK(!found);
      if (found) {
        if (restored.empty())
          prefix_set_->GetPrefixes(&restored);

        // If the item is not in the re-created list, then there is an
        // error in |PrefixSet::Exists()|.  If the item is in the
        // re-created list, then the bloom filter was wrong.
        if (std::binary_search(restored.begin(), restored.end(),
                               full_hashes[i].prefix)) {
          RecordPrefixSetInfo(PREFIX_SET_EVENT_BLOOM_MISS_PREFIX_HIT);
        } else {
          RecordPrefixSetInfo(PREFIX_SET_EVENT_BLOOM_MISS_PREFIX_HIT_INVALID);
        }
      }
    }
  }

  // If all the prefixes are cached as 'misses', don't issue a GetHash.
  if (miss_count == prefix_hits->size())
    return false;

  // Find the matching full-hash results.  |full_browse_hashes_| are from the
  // database, |pending_browse_hashes_| are from GetHash requests between
  // updates.
  std::sort(prefix_hits->begin(), prefix_hits->end());

  GetCachedFullHashesForBrowse(*prefix_hits, full_browse_hashes_,
                               full_hits, last_update);
  GetCachedFullHashesForBrowse(*prefix_hits, pending_browse_hashes_,
                               full_hits, last_update);
  return true;
}

bool SafeBrowsingDatabaseNew::MatchDownloadAddPrefixes(
    int list_bit,
    const std::vector<SBPrefix>& prefixes,
    std::vector<SBPrefix>* prefix_hits) {
  prefix_hits->clear();

  std::vector<SBAddPrefix> add_prefixes;
  download_store_->GetAddPrefixes(&add_prefixes);
  for (size_t i = 0; i < add_prefixes.size(); ++i) {
    for (size_t j = 0; j < prefixes.size(); ++j) {
      const SBPrefix& prefix = prefixes[j];
      if (prefix == add_prefixes[i].prefix &&
          GetListIdBit(add_prefixes[i].chunk_id) == list_bit) {
        prefix_hits->push_back(prefix);
      }
    }
  }
  return !prefix_hits->empty();
}

bool SafeBrowsingDatabaseNew::ContainsDownloadUrl(
    const std::vector<GURL>& urls,
    std::vector<SBPrefix>* prefix_hits) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  // Ignore this check when download checking is not enabled.
  if (!download_store_.get())
    return false;

  std::vector<SBPrefix> prefixes;
  GetDownloadUrlPrefixes(urls, &prefixes);
  return MatchDownloadAddPrefixes(safe_browsing_util::BINURL % 2,
                                  prefixes,
                                  prefix_hits);
}

bool SafeBrowsingDatabaseNew::ContainsDownloadHashPrefix(
    const SBPrefix& prefix) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  // Ignore this check when download store is not available.
  if (!download_store_.get())
    return false;

  std::vector<SBPrefix> prefixes(1, prefix);
  std::vector<SBPrefix> prefix_hits;
  return MatchDownloadAddPrefixes(safe_browsing_util::BINHASH % 2,
                                  prefixes,
                                  &prefix_hits);
}

bool SafeBrowsingDatabaseNew::ContainsCsdWhitelistedUrl(const GURL& url) {
  // This method is theoretically thread-safe but we expect all calls to
  // originate from the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::AutoLock l(lookup_lock_);
  if (csd_whitelist_all_urls_)
    return true;

  std::vector<SBFullHash> full_hashes;
  BrowseFullHashesToCheck(url, true, &full_hashes);
  for (std::vector<SBFullHash>::const_iterator it = full_hashes.begin();
       it != full_hashes.end(); ++it) {
    if (std::binary_search(csd_whitelist_.begin(), csd_whitelist_.end(), *it))
      return true;
  }
  return false;
}

// Helper to insert entries for all of the prefixes or full hashes in
// |entry| into the store.
void SafeBrowsingDatabaseNew::InsertAdd(int chunk_id, SBPrefix host,
                                        const SBEntry* entry, int list_id) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  SafeBrowsingStore* store = GetStore(list_id);
  if (!store) return;

  STATS_COUNTER("SB.HostInsert", 1);
  const int encoded_chunk_id = EncodeChunkId(chunk_id, list_id);
  const int count = entry->prefix_count();

  DCHECK(!entry->IsSub());
  if (!count) {
    // No prefixes, use host instead.
    STATS_COUNTER("SB.PrefixAdd", 1);
    store->WriteAddPrefix(encoded_chunk_id, host);
  } else if (entry->IsPrefix()) {
    // Prefixes only.
    for (int i = 0; i < count; i++) {
      const SBPrefix prefix = entry->PrefixAt(i);
      STATS_COUNTER("SB.PrefixAdd", 1);
      store->WriteAddPrefix(encoded_chunk_id, prefix);
    }
  } else {
    // Prefixes and hashes.
    const base::Time receive_time = base::Time::Now();
    for (int i = 0; i < count; ++i) {
      const SBFullHash full_hash = entry->FullHashAt(i);
      const SBPrefix prefix = full_hash.prefix;

      STATS_COUNTER("SB.PrefixAdd", 1);
      store->WriteAddPrefix(encoded_chunk_id, prefix);

      STATS_COUNTER("SB.PrefixAddFull", 1);
      store->WriteAddHash(encoded_chunk_id, receive_time, full_hash);
    }
  }
}

// Helper to iterate over all the entries in the hosts in |chunks| and
// add them to the store.
void SafeBrowsingDatabaseNew::InsertAddChunks(const int list_id,
                                              const SBChunkList& chunks) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  SafeBrowsingStore* store = GetStore(list_id);
  if (!store) return;

  for (SBChunkList::const_iterator citer = chunks.begin();
       citer != chunks.end(); ++citer) {
    const int chunk_id = citer->chunk_number;

    // The server can give us a chunk that we already have because
    // it's part of a range.  Don't add it again.
    const int encoded_chunk_id = EncodeChunkId(chunk_id, list_id);
    if (store->CheckAddChunk(encoded_chunk_id))
      continue;

    store->SetAddChunk(encoded_chunk_id);
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

  SafeBrowsingStore* store = GetStore(list_id);
  if (!store) return;

  STATS_COUNTER("SB.HostDelete", 1);
  const int encoded_chunk_id = EncodeChunkId(chunk_id, list_id);
  const int count = entry->prefix_count();

  DCHECK(entry->IsSub());
  if (!count) {
    // No prefixes, use host instead.
    STATS_COUNTER("SB.PrefixSub", 1);
    const int add_chunk_id = EncodeChunkId(entry->chunk_id(), list_id);
    store->WriteSubPrefix(encoded_chunk_id, add_chunk_id, host);
  } else if (entry->IsPrefix()) {
    // Prefixes only.
    for (int i = 0; i < count; i++) {
      const SBPrefix prefix = entry->PrefixAt(i);
      const int add_chunk_id =
          EncodeChunkId(entry->ChunkIdAtPrefix(i), list_id);

      STATS_COUNTER("SB.PrefixSub", 1);
      store->WriteSubPrefix(encoded_chunk_id, add_chunk_id, prefix);
    }
  } else {
    // Prefixes and hashes.
    for (int i = 0; i < count; ++i) {
      const SBFullHash full_hash = entry->FullHashAt(i);
      const int add_chunk_id =
          EncodeChunkId(entry->ChunkIdAtPrefix(i), list_id);

      STATS_COUNTER("SB.PrefixSub", 1);
      store->WriteSubPrefix(encoded_chunk_id, add_chunk_id, full_hash.prefix);

      STATS_COUNTER("SB.PrefixSubFull", 1);
      store->WriteSubHash(encoded_chunk_id, add_chunk_id, full_hash);
    }
  }
}

// Helper to iterate over all the entries in the hosts in |chunks| and
// add them to the store.
void SafeBrowsingDatabaseNew::InsertSubChunks(int list_id,
                                              const SBChunkList& chunks) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  SafeBrowsingStore* store = GetStore(list_id);
  if (!store) return;

  for (SBChunkList::const_iterator citer = chunks.begin();
       citer != chunks.end(); ++citer) {
    const int chunk_id = citer->chunk_number;

    // The server can give us a chunk that we already have because
    // it's part of a range.  Don't add it again.
    const int encoded_chunk_id = EncodeChunkId(chunk_id, list_id);
    if (store->CheckSubChunk(encoded_chunk_id))
      continue;

    store->SetSubChunk(encoded_chunk_id);
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
  DVLOG(2) << list_name << ": " << list_id;

  SafeBrowsingStore* store = GetStore(list_id);
  if (!store) return;

  change_detected_ = true;

  store->BeginChunk();
  if (chunks.front().is_add) {
    InsertAddChunks(list_id, chunks);
  } else {
    InsertSubChunks(list_id, chunks);
  }
  store->FinishChunk();

  UMA_HISTOGRAM_TIMES("SB2.ChunkInsert", base::Time::Now() - insert_start);
}

void SafeBrowsingDatabaseNew::DeleteChunks(
    const std::vector<SBChunkDelete>& chunk_deletes) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  if (corruption_detected_ || chunk_deletes.empty())
    return;

  const std::string& list_name = chunk_deletes.front().list_name;
  const int list_id = safe_browsing_util::GetListId(list_name);

  SafeBrowsingStore* store = GetStore(list_id);
  if (!store) return;

  change_detected_ = true;

  for (size_t i = 0; i < chunk_deletes.size(); ++i) {
    std::vector<int> chunk_numbers;
    RangesToChunks(chunk_deletes[i].chunk_del, &chunk_numbers);
    for (size_t j = 0; j < chunk_numbers.size(); ++j) {
      const int encoded_chunk_id = EncodeChunkId(chunk_numbers[j], list_id);
      if (chunk_deletes[i].is_sub_del)
        store->DeleteSubChunk(encoded_chunk_id);
      else
        store->DeleteAddChunk(encoded_chunk_id);
    }
  }
}

void SafeBrowsingDatabaseNew::CacheHashResults(
    const std::vector<SBPrefix>& prefixes,
    const std::vector<SBFullHashResult>& full_hits) {
  // This is called on the I/O thread, lock against updates.
  base::AutoLock locked(lookup_lock_);

  if (full_hits.empty()) {
    prefix_miss_cache_.insert(prefixes.begin(), prefixes.end());
    return;
  }

  // TODO(shess): SBFullHashResult and SBAddFullHash are very similar.
  // Refactor to make them identical.
  const base::Time now = base::Time::Now();
  const size_t orig_size = pending_browse_hashes_.size();
  for (std::vector<SBFullHashResult>::const_iterator iter = full_hits.begin();
       iter != full_hits.end(); ++iter) {
    const int list_id = safe_browsing_util::GetListId(iter->list_name);
    if (list_id == safe_browsing_util::MALWARE ||
        list_id == safe_browsing_util::PHISH) {
      int encoded_chunk_id = EncodeChunkId(iter->add_chunk_id, list_id);
      SBAddFullHash add_full_hash(encoded_chunk_id, now, iter->hash);
      pending_browse_hashes_.push_back(add_full_hash);
    }
  }

  // Sort new entries then merge with the previously-sorted entries.
  std::vector<SBAddFullHash>::iterator
      orig_end = pending_browse_hashes_.begin() + orig_size;
  std::sort(orig_end, pending_browse_hashes_.end(), SBAddFullHashPrefixLess);
  std::inplace_merge(pending_browse_hashes_.begin(),
                     orig_end, pending_browse_hashes_.end(),
                     SBAddFullHashPrefixLess);
}

bool SafeBrowsingDatabaseNew::UpdateStarted(
    std::vector<SBListChunkRanges>* lists) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());
  DCHECK(lists);

  // If |BeginUpdate()| fails, reset the database.
  if (!browse_store_->BeginUpdate()) {
    RecordFailure(FAILURE_BROWSE_DATABASE_UPDATE_BEGIN);
    HandleCorruptDatabase();
    return false;
  }

  if (download_store_.get() && !download_store_->BeginUpdate()) {
    RecordFailure(FAILURE_DOWNLOAD_DATABASE_UPDATE_BEGIN);
    HandleCorruptDatabase();
    return false;
  }

  if (csd_whitelist_store_.get() && !csd_whitelist_store_->BeginUpdate()) {
    RecordFailure(FAILURE_CSD_WHITELIST_DATABASE_UPDATE_BEGIN);
    HandleCorruptDatabase();
    return false;
  }

  std::vector<std::string> browse_listnames;
  browse_listnames.push_back(safe_browsing_util::kMalwareList);
  browse_listnames.push_back(safe_browsing_util::kPhishingList);
  UpdateChunkRanges(browse_store_.get(), browse_listnames, lists);

  if (download_store_.get()) {
    std::vector<std::string> download_listnames;
    download_listnames.push_back(safe_browsing_util::kBinUrlList);
    download_listnames.push_back(safe_browsing_util::kBinHashList);
    UpdateChunkRanges(download_store_.get(), download_listnames, lists);
  }

  if (csd_whitelist_store_.get()) {
    std::vector<std::string> csd_whitelist_listnames;
    csd_whitelist_listnames.push_back(safe_browsing_util::kCsdWhiteList);
    UpdateChunkRanges(csd_whitelist_store_.get(),
                      csd_whitelist_listnames, lists);
  }

  corruption_detected_ = false;
  change_detected_ = false;
  return true;
}

void SafeBrowsingDatabaseNew::UpdateFinished(bool update_succeeded) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());
  if (corruption_detected_)
    return;

  // Unroll the transaction if there was a protocol error or if the
  // transaction was empty.  This will leave the bloom filter, the
  // pending hashes, and the prefix miss cache in place.
  if (!update_succeeded || !change_detected_) {
    // Track empty updates to answer questions at http://crbug.com/72216 .
    if (update_succeeded && !change_detected_)
      UMA_HISTOGRAM_COUNTS("SB2.DatabaseUpdateKilobytes", 0);
    browse_store_->CancelUpdate();
    if (download_store_.get())
      download_store_->CancelUpdate();
    if (csd_whitelist_store_.get())
      csd_whitelist_store_->CancelUpdate();
    return;
  }

  // for download
  UpdateDownloadStore();
  // for browsing
  UpdateBrowseStore();
  // for csd whitelist
  UpdateCsdWhitelistStore();
}

void SafeBrowsingDatabaseNew::UpdateCsdWhitelistStore() {
  if (!csd_whitelist_store_.get())
    return;

  // For the csd whitelist, we don't cache and save full hashes since all
  // hashes are already full.
  std::vector<SBAddFullHash> empty_add_hashes;

  // Not needed for the csd whitelist.
  std::set<SBPrefix> empty_miss_cache;

  // Note: prefixes will not be empty.  The current data store implementation
  // stores all full-length hashes as both full and prefix hashes.
  std::vector<SBAddPrefix> prefixes;
  std::vector<SBAddFullHash> full_hashes;
  if (!csd_whitelist_store_->FinishUpdate(empty_add_hashes,
                                          empty_miss_cache,
                                          &prefixes,
                                          &full_hashes)) {
    RecordFailure(FAILURE_CSD_WHITELIST_DATABASE_UPDATE_FINISH);
    CsdWhitelistAllUrls();
    return;
  }
  LoadCsdWhitelist(full_hashes);
}

void SafeBrowsingDatabaseNew::UpdateDownloadStore() {
  if (!download_store_.get())
    return;

  // For download, we don't cache and save full hashes.
  std::vector<SBAddFullHash> empty_add_hashes;

  // For download, backend lookup happens only if a prefix is in add list.
  // No need to pass in miss cache when call FinishUpdate to caculate
  // bloomfilter false positives.
  std::set<SBPrefix> empty_miss_cache;

  // These results are not used after this call. Simply ignore the
  // returned value after FinishUpdate(...).
  std::vector<SBAddPrefix> add_prefixes_result;
  std::vector<SBAddFullHash> add_full_hashes_result;

  if (!download_store_->FinishUpdate(empty_add_hashes,
                                     empty_miss_cache,
                                     &add_prefixes_result,
                                     &add_full_hashes_result))
    RecordFailure(FAILURE_DOWNLOAD_DATABASE_UPDATE_FINISH);
  return;
}

void SafeBrowsingDatabaseNew::UpdateBrowseStore() {
  // Copy out the pending add hashes.  Copy rather than swapping in
  // case |ContainsBrowseURL()| is called before the new filter is complete.
  std::vector<SBAddFullHash> pending_add_hashes;
  {
    base::AutoLock locked(lookup_lock_);
    pending_add_hashes.insert(pending_add_hashes.end(),
                              pending_browse_hashes_.begin(),
                              pending_browse_hashes_.end());
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
  if (!browse_store_->FinishUpdate(pending_add_hashes, prefix_miss_cache_,
                                   &add_prefixes, &add_full_hashes)) {
    RecordFailure(FAILURE_BROWSE_DATABASE_UPDATE_FINISH);
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

  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(PrefixSetFromAddPrefixes(add_prefixes));

  // This needs to be in sorted order by prefix for efficient access.
  std::sort(add_full_hashes.begin(), add_full_hashes.end(),
            SBAddFullHashPrefixLess);

  // Swap in the newly built filter and cache.
  {
    base::AutoLock locked(lookup_lock_);
    full_browse_hashes_.swap(add_full_hashes);

    // TODO(shess): If |CacheHashResults()| is posted between the
    // earlier lock and this clear, those pending hashes will be lost.
    // It could be fixed by only removing hashes which were collected
    // at the earlier point.  I believe that is fail-safe as-is (the
    // hash will be fetched again).
    pending_browse_hashes_.clear();
    prefix_miss_cache_.clear();
    browse_bloom_filter_.swap(filter);
    prefix_set_.swap(prefix_set);
  }

  const base::TimeDelta bloom_gen = base::Time::Now() - before;

  // Persist the bloom filter to disk.  Since only this thread changes
  // |browse_bloom_filter_|, there is no need to lock.
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
  DVLOG(1) << "SafeBrowsingDatabaseImpl built bloom filter in "
           << bloom_gen.InMilliseconds() << " ms total.  prefix count: "
           << add_prefixes.size();
  UMA_HISTOGRAM_LONG_TIMES("SB2.BuildFilter", bloom_gen);
  UMA_HISTOGRAM_COUNTS("SB2.FilterKilobytes",
                       browse_bloom_filter_->size() / 1024);
  int64 size_64;
  if (file_util::GetFileSize(browse_filename_, &size_64))
    UMA_HISTOGRAM_COUNTS("SB2.BrowseDatabaseKilobytes",
                         static_cast<int>(size_64 / 1024));
  if (file_util::GetFileSize(download_filename_, &size_64))
    UMA_HISTOGRAM_COUNTS("SB2.DownloadDatabaseKilobytes",
                         static_cast<int>(size_64 / 1024));
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
  if (!file_util::GetFileSize(browse_filename_, &size_64) || size_64 == 0)
    return;

  if (!file_util::GetFileSize(bloom_filter_filename_, &size_64) ||
      size_64 == 0) {
    RecordFailure(FAILURE_DATABASE_FILTER_MISSING);
    return;
  }

  const base::TimeTicks before = base::TimeTicks::Now();
  browse_bloom_filter_ = BloomFilter::LoadFile(bloom_filter_filename_);
  DVLOG(1) << "SafeBrowsingDatabaseNew read bloom filter in "
           << (base::TimeTicks::Now() - before).InMilliseconds() << " ms";

  if (!browse_bloom_filter_.get())
    RecordFailure(FAILURE_DATABASE_FILTER_READ);

  // Manually re-generate the prefix set from the main database.
  // TODO(shess): Write/read for prefix set.
  std::vector<SBAddPrefix> add_prefixes;
  browse_store_->GetAddPrefixes(&add_prefixes);
  prefix_set_.reset(PrefixSetFromAddPrefixes(add_prefixes));
}

bool SafeBrowsingDatabaseNew::Delete() {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  const bool r1 = browse_store_->Delete();
  if (!r1)
    RecordFailure(FAILURE_DATABASE_STORE_DELETE);

  const bool r2 = download_store_.get() ? download_store_->Delete() : true;
  if (!r2)
    RecordFailure(FAILURE_DATABASE_STORE_DELETE);

  const bool r3 = csd_whitelist_store_.get() ?
      csd_whitelist_store_->Delete() : true;
  if (!r3)
    RecordFailure(FAILURE_DATABASE_STORE_DELETE);

  const bool r4 = file_util::Delete(bloom_filter_filename_, false);
  if (!r4)
    RecordFailure(FAILURE_DATABASE_FILTER_DELETE);
  return r1 && r2 && r3 && r4;
}

void SafeBrowsingDatabaseNew::WriteBloomFilter() {
  DCHECK_EQ(creation_loop_, MessageLoop::current());

  if (!browse_bloom_filter_.get())
    return;

  const base::TimeTicks before = base::TimeTicks::Now();
  const bool write_ok = browse_bloom_filter_->WriteFile(bloom_filter_filename_);
  DVLOG(1) << "SafeBrowsingDatabaseNew wrote bloom filter in "
           << (base::TimeTicks::Now() - before).InMilliseconds() << " ms";

  if (!write_ok)
    RecordFailure(FAILURE_DATABASE_FILTER_WRITE);
}

void SafeBrowsingDatabaseNew::CsdWhitelistAllUrls() {
  base::AutoLock locked(lookup_lock_);
  csd_whitelist_all_urls_ = true;
  csd_whitelist_.clear();
}

void SafeBrowsingDatabaseNew::LoadCsdWhitelist(
    const std::vector<SBAddFullHash>& full_hashes) {
  DCHECK_EQ(creation_loop_, MessageLoop::current());
  if (full_hashes.size() > kMaxCsdWhitelistSize) {
    CsdWhitelistAllUrls();
    return;
  }

  std::vector<SBFullHash> new_csd_whitelist;
  for (std::vector<SBAddFullHash>::const_iterator it = full_hashes.begin();
       it != full_hashes.end(); ++it) {
    new_csd_whitelist.push_back(it->full_hash);
  }
  std::sort(new_csd_whitelist.begin(), new_csd_whitelist.end());

  SBFullHash kill_switch;
  crypto::SHA256HashString(kCsdKillSwitchUrl, &kill_switch,
                           sizeof(kill_switch));
  if (std::binary_search(new_csd_whitelist.begin(), new_csd_whitelist.end(),
                         kill_switch)) {
    // The kill switch is whitelisted hence we whitelist all URLs.
    CsdWhitelistAllUrls();
  } else {
    base::AutoLock locked(lookup_lock_);
    csd_whitelist_all_urls_ = false;
    csd_whitelist_.swap(new_csd_whitelist);
  }
}
