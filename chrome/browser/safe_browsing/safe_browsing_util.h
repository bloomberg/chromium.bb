// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for the SafeBrowsing code.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_
#pragma once

#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/safe_browsing/chunk_range.h"

class GURL;

class SBEntry;

// A truncated hash's type.
typedef int SBPrefix;

// Container for holding a chunk URL and the MAC of the contents of the URL.
struct ChunkUrl {
  std::string url;
  std::string mac;
  std::string list_name;
};

// A full hash.
union SBFullHash {
  char full_hash[32];
  SBPrefix prefix;
};

inline bool operator==(const SBFullHash& lhash, const SBFullHash& rhash) {
  return memcmp(lhash.full_hash, rhash.full_hash, sizeof(SBFullHash)) == 0;
}

inline bool operator<(const SBFullHash& lhash, const SBFullHash& rhash) {
  return memcmp(lhash.full_hash, rhash.full_hash, sizeof(SBFullHash)) < 0;
}

// Container for information about a specific host in an add/sub chunk.
struct SBChunkHost {
  SBPrefix host;
  SBEntry* entry;
};

// Container for an add/sub chunk.
struct SBChunk {
  SBChunk();
  ~SBChunk();

  int chunk_number;
  int list_id;
  bool is_add;
  std::deque<SBChunkHost> hosts;
};

// Container for a set of chunks.  Interim wrapper to replace use of
// |std::deque<SBChunk>| with something having safer memory semantics.
// management.
// TODO(shess): |SBEntry| is currently a very roundabout way to hold
// things pending storage.  It could be replaced with the structures
// used in SafeBrowsingStore, then lots of bridging code could
// dissappear.
class SBChunkList {
 public:
  SBChunkList();
  ~SBChunkList();

  // Implement that subset of the |std::deque<>| interface which
  // callers expect.
  bool empty() const { return chunks_.empty(); }
  size_t size() { return chunks_.size(); }

  void push_back(const SBChunk& chunk) { chunks_.push_back(chunk); }
  SBChunk& back() { return chunks_.back(); }
  SBChunk& front() { return chunks_.front(); }
  const SBChunk& front() const { return chunks_.front(); }

  typedef std::vector<SBChunk>::const_iterator const_iterator;
  const_iterator begin() const { return chunks_.begin(); }
  const_iterator end() const { return chunks_.end(); }

  typedef std::vector<SBChunk>::iterator iterator;
  iterator begin() { return chunks_.begin(); }
  iterator end() { return chunks_.end(); }

  SBChunk& operator[](size_t n) { return chunks_[n]; }
  const SBChunk& operator[](size_t n) const { return chunks_[n]; }

  // Calls |SBEvent::Destroy()| before clearing |chunks_|.
  void clear();

 private:
  std::vector<SBChunk> chunks_;

  DISALLOW_COPY_AND_ASSIGN(SBChunkList);
};

// Used when we get a gethash response.
struct SBFullHashResult {
  SBFullHash hash;
  std::string list_name;
  int add_chunk_id;
};

// Contains information about a list in the database.
struct SBListChunkRanges {
  explicit SBListChunkRanges(const std::string& n);

  std::string name;  // The list name.
  std::string adds;  // The ranges for add chunks.
  std::string subs;  // The ranges for sub chunks.
};

// Container for deleting chunks from the database.
struct SBChunkDelete {
  SBChunkDelete();
  ~SBChunkDelete();

  std::string list_name;
  bool is_sub_del;
  std::vector<ChunkRange> chunk_del;
};


// SBEntry ---------------------------------------------------------------------

// Holds information about the prefixes for a hostkey.  prefixes can either be
// 4 bytes (truncated hash) or 32 bytes (full hash).
// For adds:
//   [list id ][chunk id][prefix count (0..n)][prefix1][prefix2]
// For subs:
//   [list id ][chunk id (only used if prefix count is 0][prefix count (0..n)]
//       [add chunk][prefix][add chunk][prefix]
class SBEntry {
 public:
  enum Type {
    ADD_PREFIX,     // 4 byte add entry.
    SUB_PREFIX,     // 4 byte sub entry.
    ADD_FULL_HASH,  // 32 byte add entry.
    SUB_FULL_HASH,  // 32 byte sub entry.
  };

  // Creates a SBEntry with the necessary size for the given number of prefixes.
  // Caller ownes the object and needs to free it by calling Destroy.
  static SBEntry* Create(Type type, int prefix_count);

  // Frees the entry's memory.
  void Destroy();

  void set_list_id(int list_id) { data_.list_id = list_id; }
  int list_id() const { return data_.list_id; }
  void set_chunk_id(int chunk_id) { data_.chunk_id = chunk_id; }
  int chunk_id() const { return data_.chunk_id; }
  int prefix_count() const { return data_.prefix_count; }

  // Returns true if this is a prefix as opposed to a full hash.
  bool IsPrefix() const {
    return type() == ADD_PREFIX || type() == SUB_PREFIX;
  }

  // Returns true if this is an add entry.
  bool IsAdd() const {
    return type() == ADD_PREFIX || type() == ADD_FULL_HASH;
  }

  // Returns true if this is a sub entry.
  bool IsSub() const {
    return type() == SUB_PREFIX || type() == SUB_FULL_HASH;
  }

  // Helper to return the size of the prefixes.
  int HashLen() const {
    return IsPrefix() ? sizeof(SBPrefix) : sizeof(SBFullHash);
  }

  // For add entries, returns the add chunk id.  For sub entries, returns the
  // add_chunk id for the prefix at the given index.
  int ChunkIdAtPrefix(int index) const;

  // Used for sub chunks to set the chunk id at a given index.
  void SetChunkIdAtPrefix(int index, int chunk_id);

  // Return the prefix/full hash at the given index.  Caller is expected to
  // call the right function based on the hash length.
  const SBPrefix& PrefixAt(int index) const;
  const SBFullHash& FullHashAt(int index) const;

  // Return the prefix/full hash at the given index.  Caller is expected to
  // call the right function based on the hash length.
  void SetPrefixAt(int index, const SBPrefix& prefix);
  void SetFullHashAt(int index, const SBFullHash& full_hash);

 private:
  // Container for a sub prefix.
  struct SBSubPrefix {
    int add_chunk;
    SBPrefix prefix;
  };

  // Container for a sub full hash.
  struct SBSubFullHash {
    int add_chunk;
    SBFullHash prefix;
  };

  // Keep the fixed data together in one struct so that we can get its size
  // easily.  If any of this is modified, the database will have to be cleared.
  struct Data {
    int list_id;
    // For adds, this is the add chunk number.
    // For subs: if prefix_count is 0 then this is the add chunk that this sub
    //     refers to.  Otherwise it's ignored, and the add_chunk in sub_prefixes
    //     or sub_full_hashes is used for each corresponding prefix.
    int chunk_id;
    Type type;
    int prefix_count;
  };

  SBEntry();
  ~SBEntry();

  // Helper to return the size of each prefix entry (i.e. for subs this
  // includes an add chunk id).
  static int PrefixSize(Type type);

  // Helper to return how much memory a given Entry would require.
  static int Size(Type type, int prefix_count);

  // Returns how many bytes this entry is.
  int Size() const;

  Type type() const { return data_.type; }

  void set_prefix_count(int count) { data_.prefix_count = count; }
  void set_type(Type type) { data_.type = type; }

  // The prefixes union must follow the fixed data so that they're contiguous
  // in memory.
  Data data_;
  union {
    SBPrefix add_prefixes_[1];
    SBSubPrefix sub_prefixes_[1];
    SBFullHash add_full_hashes_[1];
    SBSubFullHash sub_full_hashes_[1];
  };
};


// Utility functions -----------------------------------------------------------

namespace safe_browsing_util {

// SafeBrowsing list names.
extern const char kMalwareList[];
extern const char kPhishingList[];
// Binary Download list names.
extern const char kBinUrlList[];
extern const char kBinHashList[];
// SafeBrowsing client-side detection whitelist list name.
extern const char kCsdWhiteList[];

enum ListType {
  INVALID = -1,
  MALWARE = 0,
  PHISH = 1,
  BINURL = 2,
  BINHASH = 3,
  CSDWHITELIST = 4,
};

// Maps a list name to ListType.
int GetListId(const std::string& name);
// Maps a ListId to list name. Return false if fails.
bool GetListName(int list_id, std::string* list);


// Canonicalizes url as per Google Safe Browsing Specification.
// See section 6.1 in
// http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec.
void CanonicalizeUrl(const GURL& url, std::string* canonicalized_hostname,
                     std::string* canonicalized_path,
                     std::string* canonicalized_query);

// Given a URL, returns all the hosts we need to check.  They are returned
// in order of size (i.e. b.c is first, then a.b.c).
void GenerateHostsToCheck(const GURL& url, std::vector<std::string>* hosts);

// Given a URL, returns all the paths we need to check.
void GeneratePathsToCheck(const GURL& url, std::vector<std::string>* paths);

int GetHashIndex(const SBFullHash& hash,
                 const std::vector<SBFullHashResult>& full_hashes);

// Given a URL, compare all the possible host + path full hashes to the set of
// provided full hashes.  Returns the index of the match if one is found, or -1
// otherwise.
int GetUrlHashIndex(const GURL& url,
                    const std::vector<SBFullHashResult>& full_hashes);

bool IsPhishingList(const std::string& list_name);
bool IsMalwareList(const std::string& list_name);
bool IsBadbinurlList(const std::string& list_name);
bool IsBadbinhashList(const std::string& list_name);

// Returns 'true' if 'mac' can be verified using 'key' and 'data'.
bool VerifyMAC(const std::string& key,
               const std::string& mac,
               const char* data,
               int data_length);

GURL GeneratePhishingReportUrl(const std::string& report_page,
                               const std::string& url_to_report);

void StringToSBFullHash(const std::string& hash_in, SBFullHash* hash_out);
std::string SBFullHashToString(const SBFullHash& hash_out);
}  // namespace safe_browsing_util

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_
