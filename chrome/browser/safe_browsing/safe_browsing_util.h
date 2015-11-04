// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for the SafeBrowsing code.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_

#include <cstring>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/chunk_range.h"
#include "components/safe_browsing_db/safe_browsing_db_util.h"

namespace safe_browsing {
class ChunkData;
};

class GURL;

// Container for holding a chunk URL and the list it belongs to.
struct ChunkUrl {
  std::string url;
  std::string list_name;
};

// Data for an individual chunk sent from the server.
class SBChunkData {
 public:
  SBChunkData();
  ~SBChunkData();

  // Create with manufactured data, for testing only.
  // TODO(shess): Right now the test code calling this is in an anonymous
  // namespace.  Figure out how to shift this into private:.
  explicit SBChunkData(safe_browsing::ChunkData* chunk_data);

  // Read serialized ChunkData, returning true if the parse suceeded.
  bool ParseFrom(const unsigned char* data, size_t length);

  // Access the chunk data.  |AddChunkNumberAt()| can only be called if
  // |IsSub()| returns true.  |Prefix*()| and |FullHash*()| can only be called
  // if the corrosponding |Is*()| returned true.
  int ChunkNumber() const;
  bool IsAdd() const;
  bool IsSub() const;
  int AddChunkNumberAt(size_t i) const;
  bool IsPrefix() const;
  size_t PrefixCount() const;
  SBPrefix PrefixAt(size_t i) const;
  bool IsFullHash() const;
  size_t FullHashCount() const;
  SBFullHash FullHashAt(size_t i) const;

 private:
  // Protocol buffer sent from server.
  scoped_ptr<safe_browsing::ChunkData> chunk_data_;

  DISALLOW_COPY_AND_ASSIGN(SBChunkData);
};

// Used when we get a gethash response.
struct SBFullHashResult {
  SBFullHash hash;
  // TODO(shess): Refactor to allow ListType here.
  int list_id;
  std::string metadata;
};

// Caches individual response from GETHASH request.
struct SBCachedFullHashResult {
  SBCachedFullHashResult();
  explicit SBCachedFullHashResult(const base::Time& in_expire_after);
  ~SBCachedFullHashResult();

  base::Time expire_after;
  std::vector<SBFullHashResult> full_hashes;
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

// Utility functions -----------------------------------------------------------

namespace safe_browsing_util {

// SafeBrowsing list names.
extern const char kMalwareList[];
extern const char kPhishingList[];
// Binary Download list name.
extern const char kBinUrlList[];
// SafeBrowsing client-side detection whitelist list name.
extern const char kCsdWhiteList[];
// SafeBrowsing download whitelist list name.
extern const char kDownloadWhiteList[];
// SafeBrowsing extension list name.
extern const char kExtensionBlacklist[];
// SafeBrowsing csd malware IP blacklist name.
extern const char kIPBlacklist[];
// SafeBrowsing unwanted URL list.
extern const char kUnwantedUrlList[];
// SafeBrowsing off-domain inclusion whitelist list name.
extern const char kInclusionWhitelist[];

// This array must contain all Safe Browsing lists.
extern const char* kAllLists[9];

enum ListType {
  INVALID = -1,
  MALWARE = 0,
  PHISH = 1,
  BINURL = 2,
  // Obsolete BINHASH = 3,
  CSDWHITELIST = 4,
  // SafeBrowsing lists are stored in pairs.  Keep ListType 5
  // available for a potential second list that we would store in the
  // csd-whitelist store file.
  DOWNLOADWHITELIST = 6,
  // See above comment. Leave 7 available.
  EXTENSIONBLACKLIST = 8,
  // See above comment. Leave 9 available.
  // Obsolete SIDEEFFECTFREEWHITELIST = 10,
  // See above comment. Leave 11 available.
  IPBLACKLIST = 12,
  // See above comment.  Leave 13 available.
  UNWANTEDURL = 14,
  // See above comment.  Leave 15 available.
  INCLUSIONWHITELIST = 16,
  // See above comment.  Leave 17 available.
};

// Maps a list name to ListType.
ListType GetListId(const base::StringPiece& name);

// Maps a ListId to list name. Return false if fails.
bool GetListName(ListType list_id, std::string* list);

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

// Given a URL, returns all the patterns we need to check.
void GeneratePatternsToCheck(const GURL& url, std::vector<std::string>* urls);

SBFullHash StringToSBFullHash(const std::string& hash_in);
std::string SBFullHashToString(const SBFullHash& hash_out);

}  // namespace safe_browsing_util

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_
