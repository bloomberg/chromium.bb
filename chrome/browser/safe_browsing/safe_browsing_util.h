// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities for the SafeBrowsing code.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_

#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/chunk_range.h"

namespace safe_browsing {
class ChunkData;
};

class GURL;

// A truncated hash's type.
typedef uint32 SBPrefix;

// Container for holding a chunk URL and the list it belongs to.
struct ChunkUrl {
  std::string url;
  std::string list_name;
};

// A full hash.
union SBFullHash {
  char full_hash[32];
  SBPrefix prefix;
};

inline bool SBFullHashEqual(const SBFullHash& a, const SBFullHash& b) {
  return !memcmp(a.full_hash, b.full_hash, sizeof(a.full_hash));
}

inline bool SBFullHashLess(const SBFullHash& a, const SBFullHash& b) {
  return memcmp(a.full_hash, b.full_hash, sizeof(a.full_hash)) < 0;
}

// Generate full hash for the given string.
SBFullHash SBFullHashForString(const base::StringPiece& str);

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

// Different types of threats that SafeBrowsing protects against.
enum SBThreatType {
  // No threat at all.
  SB_THREAT_TYPE_SAFE,

  // The URL is being used for phishing.
  SB_THREAT_TYPE_URL_PHISHING,

  // The URL hosts malware.
  SB_THREAT_TYPE_URL_MALWARE,

  // The download URL is malware.
  SB_THREAT_TYPE_BINARY_MALWARE_URL,

  // Url detected by the client-side phishing model.  Note that unlike the
  // above values, this does not correspond to a downloaded list.
  SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL,

  // The Chrome extension or app (given by its ID) is malware.
  SB_THREAT_TYPE_EXTENSION,

  // Url detected by the client-side malware IP list. This IP list is part
  // of the client side detection model.
  SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL,
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
// SafeBrowsing side-effect free whitelist name.
extern const char kSideEffectFreeWhitelist[];
// SafeBrowsing csd malware IP blacklist name.
extern const char kIPBlacklist[];

// This array must contain all Safe Browsing lists.
extern const char* kAllLists[8];

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
  SIDEEFFECTFREEWHITELIST = 10,
  // See above comment. Leave 11 available.
  IPBLACKLIST = 12,
  // See above comment.  Leave 13 available.
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

GURL GeneratePhishingReportUrl(const std::string& report_page,
                               const std::string& url_to_report,
                               bool is_client_side_detection);

SBFullHash StringToSBFullHash(const std::string& hash_in);
std::string SBFullHashToString(const SBFullHash& hash_out);

}  // namespace safe_browsing_util

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_UTIL_H_
