// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_store.h"

namespace base {
class MessageLoop;
}

namespace safe_browsing {
class PrefixSet;
}

class GURL;
class SafeBrowsingDatabase;

// Factory for creating SafeBrowsingDatabase. Tests implement this factory
// to create fake Databases for testing.
class SafeBrowsingDatabaseFactory {
 public:
  SafeBrowsingDatabaseFactory() { }
  virtual ~SafeBrowsingDatabaseFactory() { }
  virtual SafeBrowsingDatabase* CreateSafeBrowsingDatabase(
      bool enable_download_protection,
      bool enable_client_side_whitelist,
      bool enable_download_whitelist,
      bool enable_extension_blacklist,
      bool enable_side_effect_free_whitelist,
      bool enable_ip_blacklist) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingDatabaseFactory);
};

// Encapsulates on-disk databases that for safebrowsing. There are
// four databases: browse, download, download whitelist and
// client-side detection (csd) whitelist databases. The browse database contains
// information about phishing and malware urls. The download database contains
// URLs for bad binaries (e.g: those containing virus) and hash of
// these downloaded contents. The download whitelist contains whitelisted
// download hosting sites as well as whitelisted binary signing certificates
// etc.  The csd whitelist database contains URLs that will never be considered
// as phishing by the client-side phishing detection. These on-disk databases
// are shared among all profiles, as it doesn't contain user-specific data. This
// object is not thread-safe, i.e. all its methods should be used on the same
// thread that it was created on.
class SafeBrowsingDatabase {
 public:
  // Factory method for obtaining a SafeBrowsingDatabase implementation.
  // It is not thread safe.
  // |enable_download_protection| is used to control the download database
  // feature.
  // |enable_client_side_whitelist| is used to control the csd whitelist
  // database feature.
  // |enable_download_whitelist| is used to control the download whitelist
  // database feature.
  // |enable_ip_blacklist| is used to control the csd malware IP blacklist
  // database feature.
  static SafeBrowsingDatabase* Create(bool enable_download_protection,
                                      bool enable_client_side_whitelist,
                                      bool enable_download_whitelist,
                                      bool enable_extension_blacklist,
                                      bool side_effect_free_whitelist,
                                      bool enable_ip_blacklist);

  // Makes the passed |factory| the factory used to instantiate
  // a SafeBrowsingDatabase. This is used for tests.
  static void RegisterFactory(SafeBrowsingDatabaseFactory* factory) {
    factory_ = factory;
  }

  virtual ~SafeBrowsingDatabase();

  // Initializes the database with the given filename.
  virtual void Init(const base::FilePath& filename) = 0;

  // Deletes the current database and creates a new one.
  virtual bool ResetDatabase() = 0;

  // Returns false if |url| is not in the browse database or already was cached
  // as a miss.  If it returns true, |prefix_hits| contains matching hash
  // prefixes which had no cached results and |cache_hits| contains any matching
  // cached gethash results.  This function is safe to call from any thread.
  virtual bool ContainsBrowseUrl(
      const GURL& url,
      std::vector<SBPrefix>* prefix_hits,
      std::vector<SBFullHashResult>* cache_hits) = 0;

  // Returns false if none of |urls| are in Download database. If it returns
  // true, |prefix_hits| should contain the prefixes for the URLs that were in
  // the database.  This function could ONLY be accessed from creation thread.
  virtual bool ContainsDownloadUrl(const std::vector<GURL>& urls,
                                   std::vector<SBPrefix>* prefix_hits) = 0;

  // Returns false if |url| is not on the client-side phishing detection
  // whitelist.  Otherwise, this function returns true.  Note: the whitelist
  // only contains full-length hashes so we don't return any prefix hit.
  // This function should only be called from the IO thread.
  virtual bool ContainsCsdWhitelistedUrl(const GURL& url) = 0;

  // The download whitelist is used for two purposes: a white-domain list of
  // sites that are considered to host only harmless binaries as well as a
  // whitelist of arbitrary strings such as hashed certificate authorities that
  // are considered to be trusted.  The two methods below let you lookup
  // the whitelist either for a URL or an arbitrary string.  These methods will
  // return false if no match is found and true otherwise.
  // This function could ONLY be accessed from the IO thread.
  virtual bool ContainsDownloadWhitelistedUrl(const GURL& url) = 0;
  virtual bool ContainsDownloadWhitelistedString(const std::string& str) = 0;

  // Populates |prefix_hits| with any prefixes in |prefixes| that have matches
  // in the database.
  //
  // This function can ONLY be accessed from the creation thread.
  virtual bool ContainsExtensionPrefixes(
      const std::vector<SBPrefix>& prefixes,
      std::vector<SBPrefix>* prefix_hits) = 0;

  // Returns false unless the hash of |url| is on the side-effect free
  // whitelist.
  virtual bool ContainsSideEffectFreeWhitelistUrl(const GURL& url) = 0;

  // Returns true iff the given IP is currently on the csd malware IP blacklist.
  virtual bool ContainsMalwareIP(const std::string& ip_address) = 0;

  // A database transaction should look like:
  //
  // std::vector<SBListChunkRanges> lists;
  // if (db.UpdateStarted(&lists)) {
  //   // Do something with |lists|.
  //
  //   // Process add/sub commands.
  //   db.InsertChunks(list_name, chunks);
  //
  //   // Process adddel/subdel commands.
  //   db.DeleteChunks(chunks_deletes);
  //
  //   // If passed true, processes the collected chunk info and
  //   // rebuilds the filter.  If passed false, rolls everything
  //   // back.
  //   db.UpdateFinished(success);
  // }
  //
  // If UpdateStarted() returns true, the caller MUST eventually call
  // UpdateFinished().  If it returns false, the caller MUST NOT call
  // the other functions.
  virtual bool UpdateStarted(std::vector<SBListChunkRanges>* lists) = 0;
  virtual void InsertChunks(const std::string& list_name,
                            const std::vector<SBChunkData*>& chunks) = 0;
  virtual void DeleteChunks(
      const std::vector<SBChunkDelete>& chunk_deletes) = 0;
  virtual void UpdateFinished(bool update_succeeded) = 0;

  // Store the results of a GetHash response. In the case of empty results, we
  // cache the prefixes until the next update so that we don't have to issue
  // further GetHash requests we know will be empty.
  virtual void CacheHashResults(
      const std::vector<SBPrefix>& prefixes,
      const std::vector<SBFullHashResult>& full_hits,
      const base::TimeDelta& cache_lifetime) = 0;

  // Returns true if the malware IP blacklisting killswitch URL is present
  // in the csd whitelist.
  virtual bool IsMalwareIPMatchKillSwitchOn() = 0;

  // Returns true if the whitelist killswitch URL is present in the csd
  // whitelist.
  virtual bool IsCsdWhitelistKillSwitchOn() = 0;

  // The name of the bloom-filter file for the given database file.
  // NOTE(shess): OBSOLETE.  Present for deleting stale files.
  static base::FilePath BloomFilterForFilename(
      const base::FilePath& db_filename);

  // The name of the prefix set file for the given database file.
  static base::FilePath PrefixSetForFilename(const base::FilePath& db_filename);

  // Filename for malware and phishing URL database.
  static base::FilePath BrowseDBFilename(
      const base::FilePath& db_base_filename);

  // Filename for download URL and download binary hash database.
  static base::FilePath DownloadDBFilename(
      const base::FilePath& db_base_filename);

  // Filename for client-side phishing detection whitelist databsae.
  static base::FilePath CsdWhitelistDBFilename(
      const base::FilePath& csd_whitelist_base_filename);

  // Filename for download whitelist databsae.
  static base::FilePath DownloadWhitelistDBFilename(
      const base::FilePath& download_whitelist_base_filename);

  // Filename for extension blacklist database.
  static base::FilePath ExtensionBlacklistDBFilename(
      const base::FilePath& extension_blacklist_base_filename);

  // Filename for side-effect free whitelist database.
  static base::FilePath SideEffectFreeWhitelistDBFilename(
      const base::FilePath& side_effect_free_whitelist_base_filename);

  // Filename for the csd malware IP blacklist database.
  static base::FilePath IpBlacklistDBFilename(
      const base::FilePath& ip_blacklist_base_filename);

  // Enumerate failures for histogramming purposes.  DO NOT CHANGE THE
  // ORDERING OF THESE VALUES.
  enum FailureType {
    FAILURE_DATABASE_CORRUPT,
    FAILURE_DATABASE_CORRUPT_HANDLER,
    FAILURE_BROWSE_DATABASE_UPDATE_BEGIN,
    FAILURE_BROWSE_DATABASE_UPDATE_FINISH,
    FAILURE_DATABASE_FILTER_MISSING_OBSOLETE,
    FAILURE_DATABASE_FILTER_READ_OBSOLETE,
    FAILURE_DATABASE_FILTER_WRITE_OBSOLETE,
    FAILURE_DATABASE_FILTER_DELETE,
    FAILURE_DATABASE_STORE_MISSING,
    FAILURE_DATABASE_STORE_DELETE,
    FAILURE_DOWNLOAD_DATABASE_UPDATE_BEGIN,
    FAILURE_DOWNLOAD_DATABASE_UPDATE_FINISH,
    FAILURE_WHITELIST_DATABASE_UPDATE_BEGIN,
    FAILURE_WHITELIST_DATABASE_UPDATE_FINISH,
    FAILURE_BROWSE_PREFIX_SET_MISSING,
    FAILURE_BROWSE_PREFIX_SET_READ,
    FAILURE_BROWSE_PREFIX_SET_WRITE,
    FAILURE_BROWSE_PREFIX_SET_DELETE,
    FAILURE_EXTENSION_BLACKLIST_UPDATE_BEGIN,
    FAILURE_EXTENSION_BLACKLIST_UPDATE_FINISH,
    FAILURE_EXTENSION_BLACKLIST_DELETE,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_UPDATE_BEGIN,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_UPDATE_FINISH,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_DELETE,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_PREFIX_SET_READ,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_PREFIX_SET_WRITE,
    FAILURE_SIDE_EFFECT_FREE_WHITELIST_PREFIX_SET_DELETE,
    FAILURE_IP_BLACKLIST_UPDATE_BEGIN,
    FAILURE_IP_BLACKLIST_UPDATE_FINISH,
    FAILURE_IP_BLACKLIST_UPDATE_INVALID,
    FAILURE_IP_BLACKLIST_DELETE,

    // Memory space for histograms is determined by the max.  ALWAYS
    // ADD NEW VALUES BEFORE THIS ONE.
    FAILURE_DATABASE_MAX
  };

  static void RecordFailure(FailureType failure_type);

 private:
  // The factory used to instantiate a SafeBrowsingDatabase object.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingDatabase.
  static SafeBrowsingDatabaseFactory* factory_;
};

class SafeBrowsingDatabaseNew : public SafeBrowsingDatabase {
 public:
  // Create a database with a browse, download, download whitelist and
  // csd whitelist store objects. Takes ownership of all the store objects.
  // When |download_store| is NULL, the database will ignore any operations
  // related download (url hashes and binary hashes).  The same is true for
  // the |csd_whitelist_store|, |download_whitelist_store| and
  // |ip_blacklist_store|.
  SafeBrowsingDatabaseNew(SafeBrowsingStore* browse_store,
                          SafeBrowsingStore* download_store,
                          SafeBrowsingStore* csd_whitelist_store,
                          SafeBrowsingStore* download_whitelist_store,
                          SafeBrowsingStore* extension_blacklist_store,
                          SafeBrowsingStore* side_effect_free_whitelist_store,
                          SafeBrowsingStore* ip_blacklist_store);

  // Create a database with a browse store. This is a legacy interface that
  // useds Sqlite.
  SafeBrowsingDatabaseNew();

  virtual ~SafeBrowsingDatabaseNew();

  // Implement SafeBrowsingDatabase interface.
  virtual void Init(const base::FilePath& filename) OVERRIDE;
  virtual bool ResetDatabase() OVERRIDE;
  virtual bool ContainsBrowseUrl(
      const GURL& url,
      std::vector<SBPrefix>* prefix_hits,
      std::vector<SBFullHashResult>* cache_hits) OVERRIDE;
  virtual bool ContainsDownloadUrl(const std::vector<GURL>& urls,
                                   std::vector<SBPrefix>* prefix_hits) OVERRIDE;
  virtual bool ContainsCsdWhitelistedUrl(const GURL& url) OVERRIDE;
  virtual bool ContainsDownloadWhitelistedUrl(const GURL& url) OVERRIDE;
  virtual bool ContainsDownloadWhitelistedString(
      const std::string& str) OVERRIDE;
  virtual bool ContainsExtensionPrefixes(
      const std::vector<SBPrefix>& prefixes,
      std::vector<SBPrefix>* prefix_hits) OVERRIDE;
  virtual bool ContainsSideEffectFreeWhitelistUrl(const GURL& url)  OVERRIDE;
  virtual bool ContainsMalwareIP(const std::string& ip_address) OVERRIDE;
  virtual bool UpdateStarted(std::vector<SBListChunkRanges>* lists) OVERRIDE;
  virtual void InsertChunks(const std::string& list_name,
                            const std::vector<SBChunkData*>& chunks) OVERRIDE;
  virtual void DeleteChunks(
      const std::vector<SBChunkDelete>& chunk_deletes) OVERRIDE;
  virtual void UpdateFinished(bool update_succeeded) OVERRIDE;
  virtual void CacheHashResults(
      const std::vector<SBPrefix>& prefixes,
      const std::vector<SBFullHashResult>& full_hits,
      const base::TimeDelta& cache_lifetime) OVERRIDE;

  // Returns the value of malware_kill_switch_;
  virtual bool IsMalwareIPMatchKillSwitchOn() OVERRIDE;

  // Returns true if the CSD whitelist has everything whitelisted.
  virtual bool IsCsdWhitelistKillSwitchOn() OVERRIDE;

 private:
  friend class SafeBrowsingDatabaseTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest, HashCaching);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest, CachedFullMiss);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest, CachedPrefixHitFullMiss);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest, BrowseFullHashMatching);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingDatabaseTest,
                           BrowseFullHashAndPrefixMatching);

  // A SafeBrowsing whitelist contains a list of whitelisted full-hashes (stored
  // in a sorted vector) as well as a boolean flag indicating whether all
  // lookups in the whitelist should be considered matches for safety.
  typedef std::pair<std::vector<SBFullHash>, bool> SBWhitelist;

  // This map holds a csd malware IP blacklist which maps a prefix mask
  // to a set of hashed blacklisted IP prefixes.  Each IP prefix is a hashed
  // IPv6 IP prefix using SHA-1.
  typedef std::map<std::string, base::hash_set<std::string> > IPBlacklist;

  // Helper for ContainsBrowseUrl, exposed for testing.
  bool ContainsBrowseUrlHashes(const std::vector<SBFullHash>& full_hashes,
                               std::vector<SBPrefix>* prefix_hits,
                               std::vector<SBFullHashResult>* cache_hits);

  // Returns true if the whitelist is disabled or if any of the given hashes
  // matches the whitelist.
  bool ContainsWhitelistedHashes(const SBWhitelist& whitelist,
                                 const std::vector<SBFullHash>& hashes);

  // Return the browse_store_, download_store_, download_whitelist_store or
  // csd_whitelist_store_ based on list_id.
  SafeBrowsingStore* GetStore(int list_id);

  // Deletes the files on disk.
  bool Delete();

  // Load the prefix set off disk, if available.
  void LoadPrefixSet();

  // Writes the current prefix set to disk.
  void WritePrefixSet();

  // Loads the given full-length hashes to the given whitelist.  If the number
  // of hashes is too large or if the kill switch URL is on the whitelist
  // we will whitelist everything.
  void LoadWhitelist(const std::vector<SBAddFullHash>& full_hashes,
                     SBWhitelist* whitelist);

  // Call this method if an error occured with the given whitelist.  This will
  // result in all lookups to the whitelist to return true.
  void WhitelistEverything(SBWhitelist* whitelist);

  // Parses the IP blacklist from the given full-length hashes.
  void LoadIpBlacklist(const std::vector<SBAddFullHash>& full_hashes);

  // Helpers for handling database corruption.
  // |OnHandleCorruptDatabase()| runs |ResetDatabase()| and sets
  // |corruption_detected_|, |HandleCorruptDatabase()| posts
  // |OnHandleCorruptDatabase()| to the current thread, to be run
  // after the current task completes.
  // TODO(shess): Wire things up to entirely abort the update
  // transaction when this happens.
  void HandleCorruptDatabase();
  void OnHandleCorruptDatabase();

  // Helpers for InsertChunks().
  void InsertAddChunk(SafeBrowsingStore* store,
                      safe_browsing_util::ListType list_id,
                      const SBChunkData& chunk);
  void InsertSubChunk(SafeBrowsingStore* store,
                      safe_browsing_util::ListType list_id,
                      const SBChunkData& chunk);

  // Returns the size in bytes of the store after the update.
  int64 UpdateHashPrefixStore(const base::FilePath& store_filename,
                               SafeBrowsingStore* store,
                               FailureType failure_type);
  void UpdateBrowseStore();
  void UpdateSideEffectFreeWhitelistStore();
  void UpdateWhitelistStore(const base::FilePath& store_filename,
                            SafeBrowsingStore* store,
                            SBWhitelist* whitelist);
  void UpdateIpBlacklistStore();

  // Used to verify that various calls are made from the thread the
  // object was created on.
  base::MessageLoop* creation_loop_;

  // Lock for protecting access to variables that may be used on the IO thread.
  // This includes |prefix_set_|, |browse_gethash_cache_|, |csd_whitelist_|.
  base::Lock lookup_lock_;

  // The base filename passed to Init(), used to generate the store and prefix
  // set filenames used to store data on disk.
  base::FilePath filename_base_;

  // Underlying persistent store for chunk data.
  // For browsing related (phishing and malware URLs) chunks and prefixes.
  scoped_ptr<SafeBrowsingStore> browse_store_;

  // For download related (download URL and binary hash) chunks and prefixes.
  scoped_ptr<SafeBrowsingStore> download_store_;

  // For the client-side phishing detection whitelist chunks and full-length
  // hashes.  This list only contains 256 bit hashes.
  scoped_ptr<SafeBrowsingStore> csd_whitelist_store_;

  // For the download whitelist chunks and full-length hashes.  This list only
  // contains 256 bit hashes.
  scoped_ptr<SafeBrowsingStore> download_whitelist_store_;

  // For extension IDs.
  scoped_ptr<SafeBrowsingStore> extension_blacklist_store_;

  // For side-effect free whitelist.
  scoped_ptr<SafeBrowsingStore> side_effect_free_whitelist_store_;

  // For IP blacklist.
  scoped_ptr<SafeBrowsingStore> ip_blacklist_store_;

  SBWhitelist csd_whitelist_;
  SBWhitelist download_whitelist_;
  SBWhitelist extension_blacklist_;

  // The IP blacklist should be small.  At most a couple hundred IPs.
  IPBlacklist ip_blacklist_;

  // Cache of gethash results for browse store. Entries should not be used if
  // they are older than their expire_after field.  Cached misses will have
  // empty full_hashes field.  Cleared on each update.
  std::map<SBPrefix, SBCachedFullHashResult> browse_gethash_cache_;

  // Set if corruption is detected during the course of an update.
  // Causes the update functions to fail with no side effects, until
  // the next call to |UpdateStarted()|.
  bool corruption_detected_;

  // Set to true if any chunks are added or deleted during an update.
  // Used to optimize away database update.
  bool change_detected_;

  // Used to check if a prefix was in the browse database.
  scoped_ptr<safe_browsing::PrefixSet> browse_prefix_set_;

  // Used to check if a prefix was in the browse database.
  scoped_ptr<safe_browsing::PrefixSet> side_effect_free_whitelist_prefix_set_;

  // Used to schedule resetting the database because of corruption.
  base::WeakPtrFactory<SafeBrowsingDatabaseNew> reset_factory_;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_DATABASE_H_
