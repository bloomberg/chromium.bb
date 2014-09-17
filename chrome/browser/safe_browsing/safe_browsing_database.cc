// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_database.h"

#include <algorithm>
#include <iterator>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/prefix_set.h"
#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "net/base/net_util.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using content::BrowserThread;

namespace {

// Filename suffix for the bloom filter.
const base::FilePath::CharType kBloomFilterFile[] =
    FILE_PATH_LITERAL(" Filter 2");
// Filename suffix for the prefix set.
const base::FilePath::CharType kPrefixSetFile[] =
    FILE_PATH_LITERAL(" Prefix Set");
// Filename suffix for download store.
const base::FilePath::CharType kDownloadDBFile[] =
    FILE_PATH_LITERAL(" Download");
// Filename suffix for client-side phishing detection whitelist store.
const base::FilePath::CharType kCsdWhitelistDBFile[] =
    FILE_PATH_LITERAL(" Csd Whitelist");
// Filename suffix for the download whitelist store.
const base::FilePath::CharType kDownloadWhitelistDBFile[] =
    FILE_PATH_LITERAL(" Download Whitelist");
// Filename suffix for the extension blacklist store.
const base::FilePath::CharType kExtensionBlacklistDBFile[] =
    FILE_PATH_LITERAL(" Extension Blacklist");
// Filename suffix for the side-effect free whitelist store.
const base::FilePath::CharType kSideEffectFreeWhitelistDBFile[] =
    FILE_PATH_LITERAL(" Side-Effect Free Whitelist");
// Filename suffix for the csd malware IP blacklist store.
const base::FilePath::CharType kIPBlacklistDBFile[] =
    FILE_PATH_LITERAL(" IP Blacklist");

// Filename suffix for browse store.
// TODO(shess): "Safe Browsing Bloom Prefix Set" is full of win.
// Unfortunately, to change the name implies lots of transition code
// for little benefit.  If/when file formats change (say to put all
// the data in one file), that would be a convenient point to rectify
// this.
// TODO(shess): This shouldn't be OS-driven <http://crbug.com/394379>
#if defined(OS_ANDROID)
// NOTE(shess): This difference is also reflected in the list name in
// safe_browsing_util.cc.
// TODO(shess): Spin up an alternate list id which can be persisted in the
// store.  Then if a mistake is made, it won't cause confusion between
// incompatible lists.
const base::FilePath::CharType kBrowseDBFile[] = FILE_PATH_LITERAL(" Mobile");
#else
const base::FilePath::CharType kBrowseDBFile[] = FILE_PATH_LITERAL(" Bloom");
#endif

// Maximum number of entries we allow in any of the whitelists.
// If a whitelist on disk contains more entries then all lookups to
// the whitelist will be considered a match.
const size_t kMaxWhitelistSize = 5000;

// If the hash of this exact expression is on a whitelist then all
// lookups to this whitelist will be considered a match.
const char kWhitelistKillSwitchUrl[] =
    "sb-ssl.google.com/safebrowsing/csd/killswitch";  // Don't change this!

// If the hash of this exact expression is on a whitelist then the
// malware IP blacklisting feature will be disabled in csd.
// Don't change this!
const char kMalwareIPKillSwitchUrl[] =
    "sb-ssl.google.com/safebrowsing/csd/killswitch_malware";

const size_t kMaxIpPrefixSize = 128;
const size_t kMinIpPrefixSize = 1;

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
// other lists.  We'll also always add a pattern for the empty path.
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
      full_hashes->push_back(SBFullHashForString(hosts[i] + path));

      // We may have /foo as path-prefix in the whitelist which should
      // also match with /foo/bar and /foo?bar.  Hence, for every path
      // that ends in '/' we also add the path without the slash.
      if (include_whitelist_hashes &&
          path.size() > 1 &&
          path[path.size() - 1] == '/') {
        full_hashes->push_back(
            SBFullHashForString(hosts[i] + path.substr(0, path.size() - 1)));
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

// Helper function to compare addprefixes in |store| with |prefixes|.
// The |list_bit| indicates which list (url or hash) to compare.
//
// Returns true if there is a match, |*prefix_hits| (if non-NULL) will contain
// the actual matching prefixes.
bool MatchAddPrefixes(SafeBrowsingStore* store,
                      int list_bit,
                      const std::vector<SBPrefix>& prefixes,
                      std::vector<SBPrefix>* prefix_hits) {
  prefix_hits->clear();
  bool found_match = false;

  SBAddPrefixes add_prefixes;
  store->GetAddPrefixes(&add_prefixes);
  for (SBAddPrefixes::const_iterator iter = add_prefixes.begin();
       iter != add_prefixes.end(); ++iter) {
    for (size_t j = 0; j < prefixes.size(); ++j) {
      const SBPrefix& prefix = prefixes[j];
      if (prefix == iter->prefix &&
          GetListIdBit(iter->chunk_id) == list_bit) {
        prefix_hits->push_back(prefix);
        found_match = true;
      }
    }
  }
  return found_match;
}

// This function generates a chunk range string for |chunks|. It
// outputs one chunk range string per list and writes it to the
// |list_ranges| vector.  We expect |list_ranges| to already be of the
// right size.  E.g., if |chunks| contains chunks with two different
// list ids then |list_ranges| must contain two elements.
void GetChunkRanges(const std::vector<int>& chunks,
                    std::vector<std::string>* list_ranges) {
  // Since there are 2 possible list ids, there must be exactly two
  // list ranges.  Even if the chunk data should only contain one
  // line, this code has to somehow handle corruption.
  DCHECK_EQ(2U, list_ranges->size());

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
  if (!store)
    return;

  DCHECK_GT(listnames.size(), 0U);
  DCHECK_LE(listnames.size(), 2U);
  std::vector<int> add_chunks;
  std::vector<int> sub_chunks;
  store->GetAddChunks(&add_chunks);
  store->GetSubChunks(&sub_chunks);

  // Always decode 2 ranges, even if only the first one is expected.
  // The loop below will only load as many into |lists| as |listnames|
  // indicates.
  std::vector<std::string> adds(2);
  std::vector<std::string> subs(2);
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

void UpdateChunkRangesForLists(SafeBrowsingStore* store,
                               const std::string& listname0,
                               const std::string& listname1,
                               std::vector<SBListChunkRanges>* lists) {
  std::vector<std::string> listnames;
  listnames.push_back(listname0);
  listnames.push_back(listname1);
  UpdateChunkRanges(store, listnames, lists);
}

void UpdateChunkRangesForList(SafeBrowsingStore* store,
                              const std::string& listname,
                              std::vector<SBListChunkRanges>* lists) {
  UpdateChunkRanges(store, std::vector<std::string>(1, listname), lists);
}

// This code always checks for non-zero file size.  This helper makes
// that less verbose.
int64 GetFileSizeOrZero(const base::FilePath& file_path) {
  int64 size_64;
  if (!base::GetFileSize(file_path, &size_64))
    return 0;
  return size_64;
}

// Helper for ContainsBrowseUrlHashes().  Returns true if an un-expired match
// for |full_hash| is found in |cache|, with any matches appended to |results|
// (true can be returned with zero matches).  |expire_base| is used to check the
// cache lifetime of matches, expired matches will be discarded from |cache|.
bool GetCachedFullHash(std::map<SBPrefix, SBCachedFullHashResult>* cache,
                       const SBFullHash& full_hash,
                       const base::Time& expire_base,
                       std::vector<SBFullHashResult>* results) {
  // First check if there is a valid cached result for this prefix.
  std::map<SBPrefix, SBCachedFullHashResult>::iterator
      citer = cache->find(full_hash.prefix);
  if (citer == cache->end())
    return false;

  // Remove expired entries.
  SBCachedFullHashResult& cached_result = citer->second;
  if (cached_result.expire_after <= expire_base) {
    cache->erase(citer);
    return false;
  }

  // Find full-hash matches.
  std::vector<SBFullHashResult>& cached_hashes = cached_result.full_hashes;
  for (size_t i = 0; i < cached_hashes.size(); ++i) {
    if (SBFullHashEqual(full_hash, cached_hashes[i].hash))
      results->push_back(cached_hashes[i]);
  }

  return true;
}

}  // namespace

// The default SafeBrowsingDatabaseFactory.
class SafeBrowsingDatabaseFactoryImpl : public SafeBrowsingDatabaseFactory {
 public:
  virtual SafeBrowsingDatabase* CreateSafeBrowsingDatabase(
      bool enable_download_protection,
      bool enable_client_side_whitelist,
      bool enable_download_whitelist,
      bool enable_extension_blacklist,
      bool enable_side_effect_free_whitelist,
      bool enable_ip_blacklist) OVERRIDE {
    return new SafeBrowsingDatabaseNew(
        new SafeBrowsingStoreFile,
        enable_download_protection ? new SafeBrowsingStoreFile : NULL,
        enable_client_side_whitelist ? new SafeBrowsingStoreFile : NULL,
        enable_download_whitelist ? new SafeBrowsingStoreFile : NULL,
        enable_extension_blacklist ? new SafeBrowsingStoreFile : NULL,
        enable_side_effect_free_whitelist ? new SafeBrowsingStoreFile : NULL,
        enable_ip_blacklist ? new SafeBrowsingStoreFile : NULL);
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
    bool enable_client_side_whitelist,
    bool enable_download_whitelist,
    bool enable_extension_blacklist,
    bool enable_side_effect_free_whitelist,
    bool enable_ip_blacklist) {
  if (!factory_)
    factory_ = new SafeBrowsingDatabaseFactoryImpl();
  return factory_->CreateSafeBrowsingDatabase(
      enable_download_protection,
      enable_client_side_whitelist,
      enable_download_whitelist,
      enable_extension_blacklist,
      enable_side_effect_free_whitelist,
      enable_ip_blacklist);
}

SafeBrowsingDatabase::~SafeBrowsingDatabase() {
}

// static
base::FilePath SafeBrowsingDatabase::BrowseDBFilename(
    const base::FilePath& db_base_filename) {
  return base::FilePath(db_base_filename.value() + kBrowseDBFile);
}

// static
base::FilePath SafeBrowsingDatabase::DownloadDBFilename(
    const base::FilePath& db_base_filename) {
  return base::FilePath(db_base_filename.value() + kDownloadDBFile);
}

// static
base::FilePath SafeBrowsingDatabase::BloomFilterForFilename(
    const base::FilePath& db_filename) {
  return base::FilePath(db_filename.value() + kBloomFilterFile);
}

// static
base::FilePath SafeBrowsingDatabase::PrefixSetForFilename(
    const base::FilePath& db_filename) {
  return base::FilePath(db_filename.value() + kPrefixSetFile);
}

// static
base::FilePath SafeBrowsingDatabase::CsdWhitelistDBFilename(
    const base::FilePath& db_filename) {
  return base::FilePath(db_filename.value() + kCsdWhitelistDBFile);
}

// static
base::FilePath SafeBrowsingDatabase::DownloadWhitelistDBFilename(
    const base::FilePath& db_filename) {
  return base::FilePath(db_filename.value() + kDownloadWhitelistDBFile);
}

// static
base::FilePath SafeBrowsingDatabase::ExtensionBlacklistDBFilename(
    const base::FilePath& db_filename) {
  return base::FilePath(db_filename.value() + kExtensionBlacklistDBFile);
}

// static
base::FilePath SafeBrowsingDatabase::SideEffectFreeWhitelistDBFilename(
    const base::FilePath& db_filename) {
  return base::FilePath(db_filename.value() + kSideEffectFreeWhitelistDBFile);
}

// static
base::FilePath SafeBrowsingDatabase::IpBlacklistDBFilename(
    const base::FilePath& db_filename) {
  return base::FilePath(db_filename.value() + kIPBlacklistDBFile);
}

SafeBrowsingStore* SafeBrowsingDatabaseNew::GetStore(const int list_id) {
  if (list_id == safe_browsing_util::PHISH ||
      list_id == safe_browsing_util::MALWARE) {
    return browse_store_.get();
  } else if (list_id == safe_browsing_util::BINURL) {
    return download_store_.get();
  } else if (list_id == safe_browsing_util::CSDWHITELIST) {
    return csd_whitelist_store_.get();
  } else if (list_id == safe_browsing_util::DOWNLOADWHITELIST) {
    return download_whitelist_store_.get();
  } else if (list_id == safe_browsing_util::EXTENSIONBLACKLIST) {
    return extension_blacklist_store_.get();
  } else if (list_id == safe_browsing_util::SIDEEFFECTFREEWHITELIST) {
    return side_effect_free_whitelist_store_.get();
  } else if (list_id == safe_browsing_util::IPBLACKLIST) {
    return ip_blacklist_store_.get();
  }
  return NULL;
}

// static
void SafeBrowsingDatabase::RecordFailure(FailureType failure_type) {
  UMA_HISTOGRAM_ENUMERATION("SB2.DatabaseFailure", failure_type,
                            FAILURE_DATABASE_MAX);
}

SafeBrowsingDatabaseNew::SafeBrowsingDatabaseNew()
    : creation_loop_(base::MessageLoop::current()),
      browse_store_(new SafeBrowsingStoreFile),
      corruption_detected_(false),
      change_detected_(false),
      reset_factory_(this) {
  DCHECK(browse_store_.get());
  DCHECK(!download_store_.get());
  DCHECK(!csd_whitelist_store_.get());
  DCHECK(!download_whitelist_store_.get());
  DCHECK(!extension_blacklist_store_.get());
  DCHECK(!side_effect_free_whitelist_store_.get());
  DCHECK(!ip_blacklist_store_.get());
}

SafeBrowsingDatabaseNew::SafeBrowsingDatabaseNew(
    SafeBrowsingStore* browse_store,
    SafeBrowsingStore* download_store,
    SafeBrowsingStore* csd_whitelist_store,
    SafeBrowsingStore* download_whitelist_store,
    SafeBrowsingStore* extension_blacklist_store,
    SafeBrowsingStore* side_effect_free_whitelist_store,
    SafeBrowsingStore* ip_blacklist_store)
    : creation_loop_(base::MessageLoop::current()),
      browse_store_(browse_store),
      download_store_(download_store),
      csd_whitelist_store_(csd_whitelist_store),
      download_whitelist_store_(download_whitelist_store),
      extension_blacklist_store_(extension_blacklist_store),
      side_effect_free_whitelist_store_(side_effect_free_whitelist_store),
      ip_blacklist_store_(ip_blacklist_store),
      corruption_detected_(false),
      reset_factory_(this) {
  DCHECK(browse_store_.get());
}

SafeBrowsingDatabaseNew::~SafeBrowsingDatabaseNew() {
  // The DCHECK is disabled due to crbug.com/338486 .
  // DCHECK_EQ(creation_loop_, base::MessageLoop::current());
}

void SafeBrowsingDatabaseNew::Init(const base::FilePath& filename_base) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());

  // This should not be run multiple times.
  DCHECK(filename_base_.empty());

  filename_base_ = filename_base;

  // TODO(shess): The various stores are really only necessary while doing
  // updates, or when querying a store directly (see |ContainsDownloadUrl()|).
  // The store variables are also tested to see if a list is enabled.  Perhaps
  // the stores could be refactored into an update object so that they are only
  // live in memory while being actively used.  The sense of enabled probably
  // belongs in protocol_manager or database_manager.

  browse_store_->Init(
      BrowseDBFilename(filename_base_),
      base::Bind(&SafeBrowsingDatabaseNew::HandleCorruptDatabase,
                 base::Unretained(this)));

  {
    // NOTE: There is no need to grab the lock in this function, since
    // until it returns, there are no pointers to this class on other
    // threads.  Then again, that means there is no possibility of
    // contention on the lock...
    base::AutoLock locked(lookup_lock_);
    browse_gethash_cache_.clear();
    LoadPrefixSet();
  }

  if (download_store_.get()) {
    download_store_->Init(
        DownloadDBFilename(filename_base_),
        base::Bind(&SafeBrowsingDatabaseNew::HandleCorruptDatabase,
                   base::Unretained(this)));
  }

  if (csd_whitelist_store_.get()) {
    csd_whitelist_store_->Init(
        CsdWhitelistDBFilename(filename_base_),
        base::Bind(&SafeBrowsingDatabaseNew::HandleCorruptDatabase,
                   base::Unretained(this)));

    std::vector<SBAddFullHash> full_hashes;
    if (csd_whitelist_store_->GetAddFullHashes(&full_hashes)) {
      LoadWhitelist(full_hashes, &csd_whitelist_);
    } else {
      WhitelistEverything(&csd_whitelist_);
    }
  } else {
    WhitelistEverything(&csd_whitelist_);  // Just to be safe.
  }

  if (download_whitelist_store_.get()) {
    download_whitelist_store_->Init(
        DownloadWhitelistDBFilename(filename_base_),
        base::Bind(&SafeBrowsingDatabaseNew::HandleCorruptDatabase,
                   base::Unretained(this)));

    std::vector<SBAddFullHash> full_hashes;
    if (download_whitelist_store_->GetAddFullHashes(&full_hashes)) {
      LoadWhitelist(full_hashes, &download_whitelist_);
    } else {
      WhitelistEverything(&download_whitelist_);
    }
  } else {
    WhitelistEverything(&download_whitelist_);  // Just to be safe.
  }

  if (extension_blacklist_store_.get()) {
    extension_blacklist_store_->Init(
        ExtensionBlacklistDBFilename(filename_base_),
        base::Bind(&SafeBrowsingDatabaseNew::HandleCorruptDatabase,
                   base::Unretained(this)));
  }

  if (side_effect_free_whitelist_store_.get()) {
    const base::FilePath side_effect_free_whitelist_filename =
        SideEffectFreeWhitelistDBFilename(filename_base_);
    const base::FilePath side_effect_free_whitelist_prefix_set_filename =
        PrefixSetForFilename(side_effect_free_whitelist_filename);
    side_effect_free_whitelist_store_->Init(
        side_effect_free_whitelist_filename,
        base::Bind(&SafeBrowsingDatabaseNew::HandleCorruptDatabase,
                   base::Unretained(this)));

    // Only use the prefix set if database is present and non-empty.
    if (GetFileSizeOrZero(side_effect_free_whitelist_filename)) {
      const base::TimeTicks before = base::TimeTicks::Now();
      side_effect_free_whitelist_prefix_set_ =
          safe_browsing::PrefixSet::LoadFile(
              side_effect_free_whitelist_prefix_set_filename);
      UMA_HISTOGRAM_TIMES("SB2.SideEffectFreeWhitelistPrefixSetLoad",
                          base::TimeTicks::Now() - before);
      if (!side_effect_free_whitelist_prefix_set_.get())
        RecordFailure(FAILURE_SIDE_EFFECT_FREE_WHITELIST_PREFIX_SET_READ);
    }
  } else {
    // Delete any files of the side-effect free sidelist that may be around
    // from when it was previously enabled.
    SafeBrowsingStoreFile::DeleteStore(
        SideEffectFreeWhitelistDBFilename(filename_base_));
    base::DeleteFile(
        PrefixSetForFilename(SideEffectFreeWhitelistDBFilename(filename_base_)),
        false);
  }

  if (ip_blacklist_store_.get()) {
    ip_blacklist_store_->Init(
        IpBlacklistDBFilename(filename_base_),
        base::Bind(&SafeBrowsingDatabaseNew::HandleCorruptDatabase,
                   base::Unretained(this)));

    std::vector<SBAddFullHash> full_hashes;
    if (ip_blacklist_store_->GetAddFullHashes(&full_hashes)) {
      LoadIpBlacklist(full_hashes);
    } else {
      LoadIpBlacklist(std::vector<SBAddFullHash>());  // Clear the list.
    }
  }
}

bool SafeBrowsingDatabaseNew::ResetDatabase() {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());

  // Delete files on disk.
  // TODO(shess): Hard to see where one might want to delete without a
  // reset.  Perhaps inline |Delete()|?
  if (!Delete())
    return false;

  // Reset objects in memory.
  {
    base::AutoLock locked(lookup_lock_);
    browse_gethash_cache_.clear();
    browse_prefix_set_.reset();
    side_effect_free_whitelist_prefix_set_.reset();
    ip_blacklist_.clear();
  }
  // Wants to acquire the lock itself.
  WhitelistEverything(&csd_whitelist_);
  WhitelistEverything(&download_whitelist_);
  return true;
}

bool SafeBrowsingDatabaseNew::ContainsBrowseUrl(
    const GURL& url,
    std::vector<SBPrefix>* prefix_hits,
    std::vector<SBFullHashResult>* cache_hits) {
  // Clear the results first.
  prefix_hits->clear();
  cache_hits->clear();

  std::vector<SBFullHash> full_hashes;
  BrowseFullHashesToCheck(url, false, &full_hashes);
  if (full_hashes.empty())
    return false;

  return ContainsBrowseUrlHashes(full_hashes, prefix_hits, cache_hits);
}

bool SafeBrowsingDatabaseNew::ContainsBrowseUrlHashes(
    const std::vector<SBFullHash>& full_hashes,
    std::vector<SBPrefix>* prefix_hits,
    std::vector<SBFullHashResult>* cache_hits) {
  // Used to determine cache expiration.
  const base::Time now = base::Time::Now();

  // This function is called on the I/O thread, prevent changes to
  // filter and caches.
  base::AutoLock locked(lookup_lock_);

  // |browse_prefix_set_| is empty until it is either read from disk, or the
  // first update populates it.  Bail out without a hit if not yet
  // available.
  if (!browse_prefix_set_.get())
    return false;

  for (size_t i = 0; i < full_hashes.size(); ++i) {
    if (!GetCachedFullHash(&browse_gethash_cache_,
                           full_hashes[i],
                           now,
                           cache_hits)) {
      // No valid cached result, check the database.
      if (browse_prefix_set_->Exists(full_hashes[i]))
        prefix_hits->push_back(full_hashes[i].prefix);
    }
  }

  // Multiple full hashes could share prefix, remove duplicates.
  std::sort(prefix_hits->begin(), prefix_hits->end());
  prefix_hits->erase(std::unique(prefix_hits->begin(), prefix_hits->end()),
                     prefix_hits->end());

  return !prefix_hits->empty() || !cache_hits->empty();
}

bool SafeBrowsingDatabaseNew::ContainsDownloadUrl(
    const std::vector<GURL>& urls,
    std::vector<SBPrefix>* prefix_hits) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());

  // Ignore this check when download checking is not enabled.
  if (!download_store_.get())
    return false;

  std::vector<SBPrefix> prefixes;
  GetDownloadUrlPrefixes(urls, &prefixes);
  return MatchAddPrefixes(download_store_.get(),
                          safe_browsing_util::BINURL % 2,
                          prefixes,
                          prefix_hits);
}

bool SafeBrowsingDatabaseNew::ContainsCsdWhitelistedUrl(const GURL& url) {
  // This method is theoretically thread-safe but we expect all calls to
  // originate from the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::vector<SBFullHash> full_hashes;
  BrowseFullHashesToCheck(url, true, &full_hashes);
  return ContainsWhitelistedHashes(csd_whitelist_, full_hashes);
}

bool SafeBrowsingDatabaseNew::ContainsDownloadWhitelistedUrl(const GURL& url) {
  std::vector<SBFullHash> full_hashes;
  BrowseFullHashesToCheck(url, true, &full_hashes);
  return ContainsWhitelistedHashes(download_whitelist_, full_hashes);
}

bool SafeBrowsingDatabaseNew::ContainsExtensionPrefixes(
    const std::vector<SBPrefix>& prefixes,
    std::vector<SBPrefix>* prefix_hits) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());
  if (!extension_blacklist_store_)
    return false;

  return MatchAddPrefixes(extension_blacklist_store_.get(),
                          safe_browsing_util::EXTENSIONBLACKLIST % 2,
                          prefixes,
                          prefix_hits);
}

bool SafeBrowsingDatabaseNew::ContainsSideEffectFreeWhitelistUrl(
    const GURL& url) {
  std::string host;
  std::string path;
  std::string query;
  safe_browsing_util::CanonicalizeUrl(url, &host, &path, &query);
  std::string url_to_check = host + path;
  if (!query.empty())
    url_to_check +=  "?" + query;
  SBFullHash full_hash = SBFullHashForString(url_to_check);

  // This function can be called on any thread, so lock against any changes
  base::AutoLock locked(lookup_lock_);

  // |side_effect_free_whitelist_prefix_set_| is empty until it is either read
  // from disk, or the first update populates it.  Bail out without a hit if
  // not yet available.
  if (!side_effect_free_whitelist_prefix_set_.get())
    return false;

  return side_effect_free_whitelist_prefix_set_->Exists(full_hash);
}

bool SafeBrowsingDatabaseNew::ContainsMalwareIP(const std::string& ip_address) {
  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber(ip_address, &ip_number))
    return false;
  if (ip_number.size() == net::kIPv4AddressSize)
    ip_number = net::ConvertIPv4NumberToIPv6Number(ip_number);
  if (ip_number.size() != net::kIPv6AddressSize)
    return false;  // better safe than sorry.

  // This function can be called from any thread.
  base::AutoLock locked(lookup_lock_);
  for (IPBlacklist::const_iterator it = ip_blacklist_.begin();
       it != ip_blacklist_.end();
       ++it) {
    const std::string& mask = it->first;
    DCHECK_EQ(mask.size(), ip_number.size());
    std::string subnet(net::kIPv6AddressSize, '\0');
    for (size_t i = 0; i < net::kIPv6AddressSize; ++i) {
      subnet[i] = ip_number[i] & mask[i];
    }
    const std::string hash = base::SHA1HashString(subnet);
    DVLOG(2) << "Lookup Malware IP: "
             << " ip:" << ip_address
             << " mask:" << base::HexEncode(mask.data(), mask.size())
             << " subnet:" << base::HexEncode(subnet.data(), subnet.size())
             << " hash:" << base::HexEncode(hash.data(), hash.size());
    if (it->second.count(hash) > 0) {
      return true;
    }
  }
  return false;
}

bool SafeBrowsingDatabaseNew::ContainsDownloadWhitelistedString(
    const std::string& str) {
  std::vector<SBFullHash> hashes;
  hashes.push_back(SBFullHashForString(str));
  return ContainsWhitelistedHashes(download_whitelist_, hashes);
}

bool SafeBrowsingDatabaseNew::ContainsWhitelistedHashes(
    const SBWhitelist& whitelist,
    const std::vector<SBFullHash>& hashes) {
  base::AutoLock l(lookup_lock_);
  if (whitelist.second)
    return true;
  for (std::vector<SBFullHash>::const_iterator it = hashes.begin();
       it != hashes.end(); ++it) {
    if (std::binary_search(whitelist.first.begin(), whitelist.first.end(),
                           *it, SBFullHashLess)) {
      return true;
    }
  }
  return false;
}

// Helper to insert add-chunk entries.
void SafeBrowsingDatabaseNew::InsertAddChunk(
    SafeBrowsingStore* store,
    const safe_browsing_util::ListType list_id,
    const SBChunkData& chunk_data) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());
  DCHECK(store);

  // The server can give us a chunk that we already have because
  // it's part of a range.  Don't add it again.
  const int chunk_id = chunk_data.ChunkNumber();
  const int encoded_chunk_id = EncodeChunkId(chunk_id, list_id);
  if (store->CheckAddChunk(encoded_chunk_id))
    return;

  store->SetAddChunk(encoded_chunk_id);
  if (chunk_data.IsPrefix()) {
    const size_t c = chunk_data.PrefixCount();
    for (size_t i = 0; i < c; ++i) {
      STATS_COUNTER("SB.PrefixAdd", 1);
      store->WriteAddPrefix(encoded_chunk_id, chunk_data.PrefixAt(i));
    }
  } else {
    const size_t c = chunk_data.FullHashCount();
    for (size_t i = 0; i < c; ++i) {
      STATS_COUNTER("SB.PrefixAddFull", 1);
      store->WriteAddHash(encoded_chunk_id, chunk_data.FullHashAt(i));
    }
  }
}

// Helper to insert sub-chunk entries.
void SafeBrowsingDatabaseNew::InsertSubChunk(
    SafeBrowsingStore* store,
    const safe_browsing_util::ListType list_id,
    const SBChunkData& chunk_data) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());
  DCHECK(store);

  // The server can give us a chunk that we already have because
  // it's part of a range.  Don't add it again.
  const int chunk_id = chunk_data.ChunkNumber();
  const int encoded_chunk_id = EncodeChunkId(chunk_id, list_id);
  if (store->CheckSubChunk(encoded_chunk_id))
    return;

  store->SetSubChunk(encoded_chunk_id);
  if (chunk_data.IsPrefix()) {
    const size_t c = chunk_data.PrefixCount();
    for (size_t i = 0; i < c; ++i) {
      STATS_COUNTER("SB.PrefixSub", 1);
      const int add_chunk_id = chunk_data.AddChunkNumberAt(i);
      const int encoded_add_chunk_id = EncodeChunkId(add_chunk_id, list_id);
      store->WriteSubPrefix(encoded_chunk_id, encoded_add_chunk_id,
                            chunk_data.PrefixAt(i));
    }
  } else {
    const size_t c = chunk_data.FullHashCount();
    for (size_t i = 0; i < c; ++i) {
      STATS_COUNTER("SB.PrefixSubFull", 1);
      const int add_chunk_id = chunk_data.AddChunkNumberAt(i);
      const int encoded_add_chunk_id = EncodeChunkId(add_chunk_id, list_id);
      store->WriteSubHash(encoded_chunk_id, encoded_add_chunk_id,
                          chunk_data.FullHashAt(i));
    }
  }
}

void SafeBrowsingDatabaseNew::InsertChunks(
    const std::string& list_name,
    const std::vector<SBChunkData*>& chunks) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());

  if (corruption_detected_ || chunks.empty())
    return;

  const base::TimeTicks before = base::TimeTicks::Now();

  // TODO(shess): The caller should just pass list_id.
  const safe_browsing_util::ListType list_id =
      safe_browsing_util::GetListId(list_name);

  SafeBrowsingStore* store = GetStore(list_id);
  if (!store) return;

  change_detected_ = true;

  // TODO(shess): I believe that the list is always add or sub.  Can this use
  // that productively?
  store->BeginChunk();
  for (size_t i = 0; i < chunks.size(); ++i) {
    if (chunks[i]->IsAdd()) {
      InsertAddChunk(store, list_id, *chunks[i]);
    } else if (chunks[i]->IsSub()) {
      InsertSubChunk(store, list_id, *chunks[i]);
    } else {
      NOTREACHED();
    }
  }
  store->FinishChunk();

  UMA_HISTOGRAM_TIMES("SB2.ChunkInsert", base::TimeTicks::Now() - before);
}

void SafeBrowsingDatabaseNew::DeleteChunks(
    const std::vector<SBChunkDelete>& chunk_deletes) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());

  if (corruption_detected_ || chunk_deletes.empty())
    return;

  const std::string& list_name = chunk_deletes.front().list_name;
  const safe_browsing_util::ListType list_id =
      safe_browsing_util::GetListId(list_name);

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
    const std::vector<SBFullHashResult>& full_hits,
    const base::TimeDelta& cache_lifetime) {
  const base::Time expire_after = base::Time::Now() + cache_lifetime;

  // This is called on the I/O thread, lock against updates.
  base::AutoLock locked(lookup_lock_);

  // Create or reset all cached results for these prefixes.
  for (size_t i = 0; i < prefixes.size(); ++i) {
    browse_gethash_cache_[prefixes[i]] = SBCachedFullHashResult(expire_after);
  }

  // Insert any fullhash hits. Note that there may be one, multiple, or no
  // fullhashes for any given entry in |prefixes|.
  for (size_t i = 0; i < full_hits.size(); ++i) {
    const SBPrefix prefix = full_hits[i].hash.prefix;
    browse_gethash_cache_[prefix].full_hashes.push_back(full_hits[i]);
  }
}

bool SafeBrowsingDatabaseNew::UpdateStarted(
    std::vector<SBListChunkRanges>* lists) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());
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
    RecordFailure(FAILURE_WHITELIST_DATABASE_UPDATE_BEGIN);
    HandleCorruptDatabase();
    return false;
  }

  if (download_whitelist_store_.get() &&
      !download_whitelist_store_->BeginUpdate()) {
    RecordFailure(FAILURE_WHITELIST_DATABASE_UPDATE_BEGIN);
    HandleCorruptDatabase();
    return false;
  }

  if (extension_blacklist_store_ &&
      !extension_blacklist_store_->BeginUpdate()) {
    RecordFailure(FAILURE_EXTENSION_BLACKLIST_UPDATE_BEGIN);
    HandleCorruptDatabase();
    return false;
  }

  if (side_effect_free_whitelist_store_ &&
      !side_effect_free_whitelist_store_->BeginUpdate()) {
    RecordFailure(FAILURE_SIDE_EFFECT_FREE_WHITELIST_UPDATE_BEGIN);
    HandleCorruptDatabase();
    return false;
  }

  if (ip_blacklist_store_ && !ip_blacklist_store_->BeginUpdate()) {
    RecordFailure(FAILURE_IP_BLACKLIST_UPDATE_BEGIN);
    HandleCorruptDatabase();
    return false;
  }

  {
    base::AutoLock locked(lookup_lock_);
    // Cached fullhash results must be cleared on every database update (whether
    // successful or not.)
    browse_gethash_cache_.clear();
  }

  UpdateChunkRangesForLists(browse_store_.get(),
                            safe_browsing_util::kMalwareList,
                            safe_browsing_util::kPhishingList,
                            lists);

  // NOTE(shess): |download_store_| used to contain kBinHashList, which has been
  // deprecated.  Code to delete the list from the store shows ~15k hits/day as
  // of Feb 2014, so it has been removed.  Everything _should_ be resilient to
  // extra data of that sort.
  UpdateChunkRangesForList(download_store_.get(),
                           safe_browsing_util::kBinUrlList, lists);

  UpdateChunkRangesForList(csd_whitelist_store_.get(),
                           safe_browsing_util::kCsdWhiteList, lists);

  UpdateChunkRangesForList(download_whitelist_store_.get(),
                           safe_browsing_util::kDownloadWhiteList, lists);

  UpdateChunkRangesForList(extension_blacklist_store_.get(),
                           safe_browsing_util::kExtensionBlacklist, lists);

  UpdateChunkRangesForList(side_effect_free_whitelist_store_.get(),
                           safe_browsing_util::kSideEffectFreeWhitelist, lists);

  UpdateChunkRangesForList(ip_blacklist_store_.get(),
                           safe_browsing_util::kIPBlacklist, lists);

  corruption_detected_ = false;
  change_detected_ = false;
  return true;
}

void SafeBrowsingDatabaseNew::UpdateFinished(bool update_succeeded) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());

  // The update may have failed due to corrupt storage (for instance,
  // an excessive number of invalid add_chunks and sub_chunks).
  // Double-check that the databases are valid.
  // TODO(shess): Providing a checksum for the add_chunk and sub_chunk
  // sections would allow throwing a corruption error in
  // UpdateStarted().
  if (!update_succeeded) {
    if (!browse_store_->CheckValidity())
      DLOG(ERROR) << "Safe-browsing browse database corrupt.";

    if (download_store_.get() && !download_store_->CheckValidity())
      DLOG(ERROR) << "Safe-browsing download database corrupt.";

    if (csd_whitelist_store_.get() && !csd_whitelist_store_->CheckValidity())
      DLOG(ERROR) << "Safe-browsing csd whitelist database corrupt.";

    if (download_whitelist_store_.get() &&
        !download_whitelist_store_->CheckValidity()) {
      DLOG(ERROR) << "Safe-browsing download whitelist database corrupt.";
    }

    if (extension_blacklist_store_ &&
        !extension_blacklist_store_->CheckValidity()) {
      DLOG(ERROR) << "Safe-browsing extension blacklist database corrupt.";
    }

    if (side_effect_free_whitelist_store_ &&
        !side_effect_free_whitelist_store_->CheckValidity()) {
      DLOG(ERROR) << "Safe-browsing side-effect free whitelist database "
                  << "corrupt.";
    }

    if (ip_blacklist_store_ && !ip_blacklist_store_->CheckValidity()) {
      DLOG(ERROR) << "Safe-browsing IP blacklist database corrupt.";
    }
  }

  if (corruption_detected_)
    return;

  // Unroll the transaction if there was a protocol error or if the
  // transaction was empty.  This will leave the prefix set, the
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
    if (download_whitelist_store_.get())
      download_whitelist_store_->CancelUpdate();
    if (extension_blacklist_store_)
      extension_blacklist_store_->CancelUpdate();
    if (side_effect_free_whitelist_store_)
      side_effect_free_whitelist_store_->CancelUpdate();
    if (ip_blacklist_store_)
      ip_blacklist_store_->CancelUpdate();
    return;
  }

  if (download_store_) {
    int64 size_bytes = UpdateHashPrefixStore(
        DownloadDBFilename(filename_base_),
        download_store_.get(),
        FAILURE_DOWNLOAD_DATABASE_UPDATE_FINISH);
    UMA_HISTOGRAM_COUNTS("SB2.DownloadDatabaseKilobytes",
                         static_cast<int>(size_bytes / 1024));
  }

  UpdateBrowseStore();
  UpdateWhitelistStore(CsdWhitelistDBFilename(filename_base_),
                       csd_whitelist_store_.get(),
                       &csd_whitelist_);
  UpdateWhitelistStore(DownloadWhitelistDBFilename(filename_base_),
                       download_whitelist_store_.get(),
                       &download_whitelist_);

  if (extension_blacklist_store_) {
    int64 size_bytes = UpdateHashPrefixStore(
        ExtensionBlacklistDBFilename(filename_base_),
        extension_blacklist_store_.get(),
        FAILURE_EXTENSION_BLACKLIST_UPDATE_FINISH);
    UMA_HISTOGRAM_COUNTS("SB2.ExtensionBlacklistKilobytes",
                         static_cast<int>(size_bytes / 1024));
  }

  if (side_effect_free_whitelist_store_)
    UpdateSideEffectFreeWhitelistStore();

  if (ip_blacklist_store_)
    UpdateIpBlacklistStore();
}

void SafeBrowsingDatabaseNew::UpdateWhitelistStore(
    const base::FilePath& store_filename,
    SafeBrowsingStore* store,
    SBWhitelist* whitelist) {
  if (!store)
    return;

  // Note: |builder| will not be empty.  The current data store implementation
  // stores all full-length hashes as both full and prefix hashes.
  safe_browsing::PrefixSetBuilder builder;
  std::vector<SBAddFullHash> full_hashes;
  if (!store->FinishUpdate(&builder, &full_hashes)) {
    RecordFailure(FAILURE_WHITELIST_DATABASE_UPDATE_FINISH);
    WhitelistEverything(whitelist);
    return;
  }

#if defined(OS_MACOSX)
  base::mac::SetFileBackupExclusion(store_filename);
#endif

  LoadWhitelist(full_hashes, whitelist);
}

int64 SafeBrowsingDatabaseNew::UpdateHashPrefixStore(
    const base::FilePath& store_filename,
    SafeBrowsingStore* store,
    FailureType failure_type) {
  // These results are not used after this call. Simply ignore the
  // returned value after FinishUpdate(...).
  safe_browsing::PrefixSetBuilder builder;
  std::vector<SBAddFullHash> add_full_hashes_result;

  if (!store->FinishUpdate(&builder, &add_full_hashes_result))
    RecordFailure(failure_type);

#if defined(OS_MACOSX)
  base::mac::SetFileBackupExclusion(store_filename);
#endif

  return GetFileSizeOrZero(store_filename);
}

void SafeBrowsingDatabaseNew::UpdateBrowseStore() {
  // Measure the amount of IO during the filter build.
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

  const base::TimeTicks before = base::TimeTicks::Now();

  // TODO(shess): Perhaps refactor to let builder accumulate full hashes on the
  // fly?  Other clients use the SBAddFullHash vector, but AFAICT they only use
  // the SBFullHash portion.  It would need an accessor on PrefixSet.
  safe_browsing::PrefixSetBuilder builder;
  std::vector<SBAddFullHash> add_full_hashes;
  if (!browse_store_->FinishUpdate(&builder, &add_full_hashes)) {
    RecordFailure(FAILURE_BROWSE_DATABASE_UPDATE_FINISH);
    return;
  }

  std::vector<SBFullHash> full_hash_results;
  for (size_t i = 0; i < add_full_hashes.size(); ++i) {
    full_hash_results.push_back(add_full_hashes[i].full_hash);
  }

  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(builder.GetPrefixSet(full_hash_results));

  // Swap in the newly built filter.
  {
    base::AutoLock locked(lookup_lock_);
    browse_prefix_set_.swap(prefix_set);
  }

  UMA_HISTOGRAM_LONG_TIMES("SB2.BuildFilter", base::TimeTicks::Now() - before);

  // Persist the prefix set to disk.  Since only this thread changes
  // |browse_prefix_set_|, there is no need to lock.
  WritePrefixSet();

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

  const base::FilePath browse_filename = BrowseDBFilename(filename_base_);
  const int64 file_size = GetFileSizeOrZero(browse_filename);
  UMA_HISTOGRAM_COUNTS("SB2.BrowseDatabaseKilobytes",
                       static_cast<int>(file_size / 1024));

#if defined(OS_MACOSX)
  base::mac::SetFileBackupExclusion(browse_filename);
#endif
}

void SafeBrowsingDatabaseNew::UpdateSideEffectFreeWhitelistStore() {
  safe_browsing::PrefixSetBuilder builder;
  std::vector<SBAddFullHash> add_full_hashes_result;

  if (!side_effect_free_whitelist_store_->FinishUpdate(
          &builder, &add_full_hashes_result)) {
    RecordFailure(FAILURE_SIDE_EFFECT_FREE_WHITELIST_UPDATE_FINISH);
    return;
  }
  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(builder.GetPrefixSetNoHashes());

  // Swap in the newly built prefix set.
  {
    base::AutoLock locked(lookup_lock_);
    side_effect_free_whitelist_prefix_set_.swap(prefix_set);
  }

  const base::FilePath side_effect_free_whitelist_filename =
      SideEffectFreeWhitelistDBFilename(filename_base_);
  const base::FilePath side_effect_free_whitelist_prefix_set_filename =
      PrefixSetForFilename(side_effect_free_whitelist_filename);
  const base::TimeTicks before = base::TimeTicks::Now();
  const bool write_ok = side_effect_free_whitelist_prefix_set_->WriteFile(
      side_effect_free_whitelist_prefix_set_filename);
  UMA_HISTOGRAM_TIMES("SB2.SideEffectFreePrefixSetWrite",
                      base::TimeTicks::Now() - before);

  if (!write_ok)
    RecordFailure(FAILURE_SIDE_EFFECT_FREE_WHITELIST_PREFIX_SET_WRITE);

  // Gather statistics.
  int64 file_size = GetFileSizeOrZero(
      side_effect_free_whitelist_prefix_set_filename);
  UMA_HISTOGRAM_COUNTS("SB2.SideEffectFreeWhitelistPrefixSetKilobytes",
                       static_cast<int>(file_size / 1024));
  file_size = GetFileSizeOrZero(side_effect_free_whitelist_filename);
  UMA_HISTOGRAM_COUNTS("SB2.SideEffectFreeWhitelistDatabaseKilobytes",
                       static_cast<int>(file_size / 1024));

#if defined(OS_MACOSX)
  base::mac::SetFileBackupExclusion(side_effect_free_whitelist_filename);
  base::mac::SetFileBackupExclusion(
      side_effect_free_whitelist_prefix_set_filename);
#endif
}

void SafeBrowsingDatabaseNew::UpdateIpBlacklistStore() {
  // Note: prefixes will not be empty.  The current data store implementation
  // stores all full-length hashes as both full and prefix hashes.
  safe_browsing::PrefixSetBuilder builder;
  std::vector<SBAddFullHash> full_hashes;
  if (!ip_blacklist_store_->FinishUpdate(&builder, &full_hashes)) {
    RecordFailure(FAILURE_IP_BLACKLIST_UPDATE_FINISH);
    LoadIpBlacklist(std::vector<SBAddFullHash>());  // Clear the list.
    return;
  }

#if defined(OS_MACOSX)
  base::mac::SetFileBackupExclusion(IpBlacklistDBFilename(filename_base_));
#endif

  LoadIpBlacklist(full_hashes);
}

void SafeBrowsingDatabaseNew::HandleCorruptDatabase() {
  // Reset the database after the current task has unwound (but only
  // reset once within the scope of a given task).
  if (!reset_factory_.HasWeakPtrs()) {
    RecordFailure(FAILURE_DATABASE_CORRUPT);
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&SafeBrowsingDatabaseNew::OnHandleCorruptDatabase,
                   reset_factory_.GetWeakPtr()));
  }
}

void SafeBrowsingDatabaseNew::OnHandleCorruptDatabase() {
  RecordFailure(FAILURE_DATABASE_CORRUPT_HANDLER);
  corruption_detected_ = true;  // Stop updating the database.
  ResetDatabase();

  // NOTE(shess): ResetDatabase() should remove the corruption, so this should
  // only happen once.  If you are here because you are hitting this after a
  // restart, then I would be very interested in working with you to figure out
  // what is happening, since it may affect real users.
  DLOG(FATAL) << "SafeBrowsing database was corrupt and reset";
}

// TODO(shess): I'm not clear why this code doesn't have any
// real error-handling.
void SafeBrowsingDatabaseNew::LoadPrefixSet() {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());
  DCHECK(!filename_base_.empty());

  const base::FilePath browse_filename = BrowseDBFilename(filename_base_);
  const base::FilePath browse_prefix_set_filename =
      PrefixSetForFilename(browse_filename);

  // Only use the prefix set if database is present and non-empty.
  if (!GetFileSizeOrZero(browse_filename))
    return;

  // Cleanup any stale bloom filter (no longer used).
  // TODO(shess): Track existence to drive removal of this code?
  const base::FilePath bloom_filter_filename =
      BloomFilterForFilename(browse_filename);
  base::DeleteFile(bloom_filter_filename, false);

  const base::TimeTicks before = base::TimeTicks::Now();
  browse_prefix_set_ = safe_browsing::PrefixSet::LoadFile(
      browse_prefix_set_filename);
  UMA_HISTOGRAM_TIMES("SB2.PrefixSetLoad", base::TimeTicks::Now() - before);

  if (!browse_prefix_set_.get())
    RecordFailure(FAILURE_BROWSE_PREFIX_SET_READ);
}

bool SafeBrowsingDatabaseNew::Delete() {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());
  DCHECK(!filename_base_.empty());

  // TODO(shess): This is a mess.  SafeBrowsingFileStore::Delete() closes the
  // store before calling DeleteStore().  DeleteStore() deletes transient files
  // in addition to the main file.  Probably all of these should be converted to
  // a helper which calls Delete() if the store exists, else DeleteStore() on
  // the generated filename.

  // TODO(shess): Determine if the histograms are useful in any way.  I cannot
  // recall any action taken as a result of their values, in which case it might
  // make more sense to histogram an overall thumbs-up/-down and just dig deeper
  // if something looks wrong.

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

  const bool r4 = download_whitelist_store_.get() ?
      download_whitelist_store_->Delete() : true;
  if (!r4)
    RecordFailure(FAILURE_DATABASE_STORE_DELETE);

  const base::FilePath browse_filename = BrowseDBFilename(filename_base_);
  const base::FilePath bloom_filter_filename =
      BloomFilterForFilename(browse_filename);
  const bool r5 = base::DeleteFile(bloom_filter_filename, false);
  if (!r5)
    RecordFailure(FAILURE_DATABASE_FILTER_DELETE);

  const base::FilePath browse_prefix_set_filename =
      PrefixSetForFilename(browse_filename);
  const bool r6 = base::DeleteFile(browse_prefix_set_filename, false);
  if (!r6)
    RecordFailure(FAILURE_BROWSE_PREFIX_SET_DELETE);

  const base::FilePath extension_blacklist_filename =
      ExtensionBlacklistDBFilename(filename_base_);
  const bool r7 = base::DeleteFile(extension_blacklist_filename, false);
  if (!r7)
    RecordFailure(FAILURE_EXTENSION_BLACKLIST_DELETE);

  const base::FilePath side_effect_free_whitelist_filename =
      SideEffectFreeWhitelistDBFilename(filename_base_);
  const bool r8 = base::DeleteFile(side_effect_free_whitelist_filename,
                                   false);
  if (!r8)
    RecordFailure(FAILURE_SIDE_EFFECT_FREE_WHITELIST_DELETE);

  const base::FilePath side_effect_free_whitelist_prefix_set_filename =
      PrefixSetForFilename(side_effect_free_whitelist_filename);
  const bool r9 = base::DeleteFile(
      side_effect_free_whitelist_prefix_set_filename,
      false);
  if (!r9)
    RecordFailure(FAILURE_SIDE_EFFECT_FREE_WHITELIST_PREFIX_SET_DELETE);

  const bool r10 = base::DeleteFile(IpBlacklistDBFilename(filename_base_),
                                    false);
  if (!r10)
    RecordFailure(FAILURE_IP_BLACKLIST_DELETE);

  return r1 && r2 && r3 && r4 && r5 && r6 && r7 && r8 && r9 && r10;
}

void SafeBrowsingDatabaseNew::WritePrefixSet() {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());

  if (!browse_prefix_set_.get())
    return;

  const base::FilePath browse_filename = BrowseDBFilename(filename_base_);
  const base::FilePath browse_prefix_set_filename =
      PrefixSetForFilename(browse_filename);

  const base::TimeTicks before = base::TimeTicks::Now();
  const bool write_ok = browse_prefix_set_->WriteFile(
      browse_prefix_set_filename);
  UMA_HISTOGRAM_TIMES("SB2.PrefixSetWrite", base::TimeTicks::Now() - before);

  const int64 file_size = GetFileSizeOrZero(browse_prefix_set_filename);
  UMA_HISTOGRAM_COUNTS("SB2.PrefixSetKilobytes",
                       static_cast<int>(file_size / 1024));

  if (!write_ok)
    RecordFailure(FAILURE_BROWSE_PREFIX_SET_WRITE);

#if defined(OS_MACOSX)
  base::mac::SetFileBackupExclusion(browse_prefix_set_filename);
#endif
}

void SafeBrowsingDatabaseNew::WhitelistEverything(SBWhitelist* whitelist) {
  base::AutoLock locked(lookup_lock_);
  whitelist->second = true;
  whitelist->first.clear();
}

void SafeBrowsingDatabaseNew::LoadWhitelist(
    const std::vector<SBAddFullHash>& full_hashes,
    SBWhitelist* whitelist) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());
  if (full_hashes.size() > kMaxWhitelistSize) {
    WhitelistEverything(whitelist);
    return;
  }

  std::vector<SBFullHash> new_whitelist;
  new_whitelist.reserve(full_hashes.size());
  for (std::vector<SBAddFullHash>::const_iterator it = full_hashes.begin();
       it != full_hashes.end(); ++it) {
    new_whitelist.push_back(it->full_hash);
  }
  std::sort(new_whitelist.begin(), new_whitelist.end(), SBFullHashLess);

  SBFullHash kill_switch = SBFullHashForString(kWhitelistKillSwitchUrl);
  if (std::binary_search(new_whitelist.begin(), new_whitelist.end(),
                         kill_switch, SBFullHashLess)) {
    // The kill switch is whitelisted hence we whitelist all URLs.
    WhitelistEverything(whitelist);
  } else {
    base::AutoLock locked(lookup_lock_);
    whitelist->second = false;
    whitelist->first.swap(new_whitelist);
  }
}

void SafeBrowsingDatabaseNew::LoadIpBlacklist(
    const std::vector<SBAddFullHash>& full_hashes) {
  DCHECK_EQ(creation_loop_, base::MessageLoop::current());
  IPBlacklist new_blacklist;
  for (std::vector<SBAddFullHash>::const_iterator it = full_hashes.begin();
       it != full_hashes.end();
       ++it) {
    const char* full_hash = it->full_hash.full_hash;
    DCHECK_EQ(crypto::kSHA256Length, arraysize(it->full_hash.full_hash));
    // The format of the IP blacklist is:
    // SHA-1(IPv6 prefix) + uint8(prefix size) + 11 unused bytes.
    std::string hashed_ip_prefix(full_hash, base::kSHA1Length);
    size_t prefix_size = static_cast<uint8>(full_hash[base::kSHA1Length]);
    if (prefix_size > kMaxIpPrefixSize || prefix_size < kMinIpPrefixSize) {
      RecordFailure(FAILURE_IP_BLACKLIST_UPDATE_INVALID);
      new_blacklist.clear();  // Load empty blacklist.
      break;
    }

    // We precompute the mask for the given subnet size to speed up lookups.
    // Basically we need to create a 16B long string which has the highest
    // |size| bits sets to one.
    std::string mask(net::kIPv6AddressSize, '\0');
    mask.replace(0, prefix_size / 8, prefix_size / 8, '\xFF');
    if ((prefix_size % 8) != 0) {
      mask[prefix_size / 8] = 0xFF << (8 - (prefix_size % 8));
    }
    DVLOG(2) << "Inserting malicious IP: "
             << " raw:" << base::HexEncode(full_hash, crypto::kSHA256Length)
             << " mask:" << base::HexEncode(mask.data(), mask.size())
             << " prefix_size:" << prefix_size
             << " hashed_ip:" << base::HexEncode(hashed_ip_prefix.data(),
                                                 hashed_ip_prefix.size());
    new_blacklist[mask].insert(hashed_ip_prefix);
  }

  base::AutoLock locked(lookup_lock_);
  ip_blacklist_.swap(new_blacklist);
}

bool SafeBrowsingDatabaseNew::IsMalwareIPMatchKillSwitchOn() {
  SBFullHash malware_kill_switch = SBFullHashForString(kMalwareIPKillSwitchUrl);
  std::vector<SBFullHash> full_hashes;
  full_hashes.push_back(malware_kill_switch);
  return ContainsWhitelistedHashes(csd_whitelist_, full_hashes);
}

bool SafeBrowsingDatabaseNew::IsCsdWhitelistKillSwitchOn() {
  return csd_whitelist_.second;
}
