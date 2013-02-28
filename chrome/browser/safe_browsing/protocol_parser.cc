// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Parse the data returned from the SafeBrowsing v2.1 protocol response.

#include <stdlib.h>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/protocol_parser.h"
#include "chrome/browser/safe_browsing/safe_browsing_util.h"

namespace {
// Helper function for quick scans of a line oriented protocol. Note that we use
//   std::string::assign(const charT* s, size_type n)
// to copy data into 'line'. This form of 'assign' does not call strlen on
// 'input', which is binary data and is not NULL terminated. 'input' may also
// contain valid NULL bytes in the payload, which a strlen based copy would
// truncate.
bool GetLine(const char* input, int input_len, std::string* line) {
  const char* pos = input;
  while (pos && (pos - input < input_len)) {
    if (*pos == '\n') {
      line->assign(input, pos - input);
      return true;
    }
    ++pos;
  }
  return false;
}
}

//------------------------------------------------------------------------------
// SafeBrowsingParser implementation

SafeBrowsingProtocolParser::SafeBrowsingProtocolParser() {
}

bool SafeBrowsingProtocolParser::ParseGetHash(
    const char* chunk_data,
    int chunk_len,
    std::vector<SBFullHashResult>* full_hashes) {
  full_hashes->clear();
  int length = chunk_len;
  const char* data = chunk_data;

  int offset;
  std::string line;
  while (length > 0) {
    if (!GetLine(data, length, &line))
      return false;

    offset = static_cast<int>(line.size()) + 1;
    data += offset;
    length -= offset;

    std::vector<std::string> cmd_parts;
    base::SplitString(line, ':', &cmd_parts);
    if (cmd_parts.size() != 3)
      return false;

    SBFullHashResult full_hash;
    full_hash.list_name = cmd_parts[0];
    full_hash.add_chunk_id = atoi(cmd_parts[1].c_str());
    int full_hash_len = atoi(cmd_parts[2].c_str());

    // Ignore hash results from lists we don't recognize.
    if (safe_browsing_util::GetListId(full_hash.list_name) < 0) {
      data += full_hash_len;
      length -= full_hash_len;
      continue;
    }

    while (full_hash_len > 0) {
      DCHECK(static_cast<size_t>(full_hash_len) >= sizeof(SBFullHash));
      memcpy(&full_hash.hash, data, sizeof(SBFullHash));
      full_hashes->push_back(full_hash);
      data += sizeof(SBFullHash);
      length -= sizeof(SBFullHash);
      full_hash_len -= sizeof(SBFullHash);
    }
  }

  return length == 0;
}

void SafeBrowsingProtocolParser::FormatGetHash(
   const std::vector<SBPrefix>& prefixes, std::string* request) {
  DCHECK(request);

  // Format the request for GetHash.
  request->append(base::StringPrintf("%" PRIuS ":%" PRIuS "\n",
                                     sizeof(SBPrefix),
                                     sizeof(SBPrefix) * prefixes.size()));
  for (size_t i = 0; i < prefixes.size(); ++i) {
    request->append(reinterpret_cast<const char*>(&prefixes[i]),
                    sizeof(SBPrefix));
  }
}

bool SafeBrowsingProtocolParser::ParseUpdate(
    const char* chunk_data,
    int chunk_len,
    int* next_update_sec,
    bool* reset,
    std::vector<SBChunkDelete>* deletes,
    std::vector<ChunkUrl>* chunk_urls) {
  DCHECK(next_update_sec);
  DCHECK(deletes);
  DCHECK(chunk_urls);

  int length = chunk_len;
  const char* data = chunk_data;

  // Populated below.
  std::string list_name;

  while (length > 0) {
    std::string cmd_line;
    if (!GetLine(data, length, &cmd_line))
      return false;  // Error: bad list format!

    std::vector<std::string> cmd_parts;
    base::SplitString(cmd_line, ':', &cmd_parts);
    if (cmd_parts.empty())
      return false;
    const std::string& command = cmd_parts[0];
    if (cmd_parts.size() != 2 && command[0] != 'u')
      return false;

    const int consumed = static_cast<int>(cmd_line.size()) + 1;
    data += consumed;
    length -= consumed;
    if (length < 0)
      return false;  // Parsing error.

    // Differentiate on the first character of the command (which is usually
    // only one character, with the exception of the 'ad' and 'sd' commands).
    switch (command[0]) {
      case 'a':
      case 's': {
        // Must be either an 'ad' (add-del) or 'sd' (sub-del) chunk. We must
        // have also parsed the list name before getting here, or the add-del
        // or sub-del will have no context.
        if (command.size() != 2 || command[1] != 'd' || list_name.empty())
          return false;
        SBChunkDelete chunk_delete;
        chunk_delete.is_sub_del = command[0] == 's';
        StringToRanges(cmd_parts[1], &chunk_delete.chunk_del);
        chunk_delete.list_name = list_name;
        deletes->push_back(chunk_delete);
        break;
      }

      case 'i':
        // The line providing the name of the list (i.e. 'goog-phish-shavar').
        list_name = cmd_parts[1];
        break;

      case 'n':
        // The line providing the next earliest time (in seconds) to re-query.
        *next_update_sec = atoi(cmd_parts[1].c_str());
        break;

      case 'u': {
        ChunkUrl chunk_url;
        chunk_url.url = cmd_line.substr(2);  // Skip the initial "u:".
        chunk_url.list_name = list_name;
        chunk_urls->push_back(chunk_url);
        break;
      }

      case 'r':
        if (cmd_parts[1] != "pleasereset")
          return false;
        *reset = true;
        break;

      default:
        // According to the spec, we ignore commands we don't understand.
        break;
    }
  }

  return true;
}

bool SafeBrowsingProtocolParser::ParseChunk(const std::string& list_name,
                                            const char* data,
                                            int length,
                                            SBChunkList* chunks) {
  int remaining = length;
  const char* chunk_data = data;

  while (remaining > 0) {
    std::string cmd_line;
    if (!GetLine(chunk_data, remaining, &cmd_line))
      return false;  // Error: bad chunk format!

    const int line_len = static_cast<int>(cmd_line.length()) + 1;
    chunk_data += line_len;
    remaining -= line_len;
    std::vector<std::string> cmd_parts;
    base::SplitString(cmd_line, ':', &cmd_parts);
    if (cmd_parts.size() != 4) {
      return false;
    }

    // Process the chunk data.
    const int chunk_number = atoi(cmd_parts[1].c_str());
    const int hash_len = atoi(cmd_parts[2].c_str());
    if (hash_len != sizeof(SBPrefix) && hash_len != sizeof(SBFullHash)) {
      VLOG(1) << "ParseChunk got unknown hashlen " << hash_len;
      return false;
    }

    const int chunk_len = atoi(cmd_parts[3].c_str());

    if (remaining < chunk_len)
      return false;  // parse error.

    chunks->push_back(SBChunk());
    chunks->back().chunk_number = chunk_number;

    if (cmd_parts[0] == "a") {
      chunks->back().is_add = true;
      if (!ParseAddChunk(list_name, chunk_data, chunk_len, hash_len,
                         &chunks->back().hosts))
        return false;  // Parse error.
    } else if (cmd_parts[0] == "s") {
      chunks->back().is_add = false;
      if (!ParseSubChunk(list_name, chunk_data, chunk_len, hash_len,
                         &chunks->back().hosts))
        return false;  // Parse error.
    } else {
      NOTREACHED();
      return false;
    }

    chunk_data += chunk_len;
    remaining -= chunk_len;
    DCHECK_LE(0, remaining);
  }

  DCHECK(remaining == 0);

  return true;
}

bool SafeBrowsingProtocolParser::ParseAddChunk(const std::string& list_name,
                                               const char* data,
                                               int data_len,
                                               int hash_len,
                                               std::deque<SBChunkHost>* hosts) {
  const char* chunk_data = data;
  int remaining = data_len;
  int prefix_count;
  SBEntry::Type type = hash_len == sizeof(SBPrefix) ?
      SBEntry::ADD_PREFIX : SBEntry::ADD_FULL_HASH;

  if (list_name == safe_browsing_util::kBinHashList ||
      list_name == safe_browsing_util::kDownloadWhiteList ||
      list_name == safe_browsing_util::kExtensionBlacklist) {
    // These lists only contain prefixes, no HOSTKEY and COUNT.
    DCHECK_EQ(0, remaining % hash_len);
    prefix_count = remaining / hash_len;
    SBChunkHost chunk_host;
    chunk_host.host = 0;
    chunk_host.entry = SBEntry::Create(type, prefix_count);
    hosts->push_back(chunk_host);
    if (!ReadPrefixes(&chunk_data, &remaining, chunk_host.entry, prefix_count))
      return false;
    DCHECK_GE(remaining, 0);
  } else {
    SBPrefix host;
    const int min_size = sizeof(SBPrefix) + 1;
    while (remaining >= min_size) {
      if (!ReadHostAndPrefixCount(&chunk_data, &remaining,
                                  &host, &prefix_count)) {
        return false;
      }
      DCHECK_GE(remaining, 0);
      SBChunkHost chunk_host;
      chunk_host.host = host;
      chunk_host.entry = SBEntry::Create(type, prefix_count);
      hosts->push_back(chunk_host);
      if (!ReadPrefixes(&chunk_data, &remaining, chunk_host.entry,
                        prefix_count))
        return false;
      DCHECK_GE(remaining, 0);
    }
  }
  return remaining == 0;
}

bool SafeBrowsingProtocolParser::ParseSubChunk(const std::string& list_name,
                                               const char* data,
                                               int data_len,
                                               int hash_len,
                                               std::deque<SBChunkHost>* hosts) {
  int remaining = data_len;
  const char* chunk_data = data;
  int prefix_count;
  SBEntry::Type type = hash_len == sizeof(SBPrefix) ?
      SBEntry::SUB_PREFIX : SBEntry::SUB_FULL_HASH;

  if (list_name == safe_browsing_util::kBinHashList ||
      list_name == safe_browsing_util::kDownloadWhiteList ||
      list_name == safe_browsing_util::kExtensionBlacklist) {
    SBChunkHost chunk_host;
    // Set host to 0 and it won't be used for kBinHashList.
    chunk_host.host = 0;
    // kBinHashList only contains (add_chunk_number, prefix) pairs, no HOSTKEY
    // and COUNT. |add_chunk_number| is int32.
    prefix_count = remaining / (sizeof(int32) + hash_len);
    chunk_host.entry = SBEntry::Create(type, prefix_count);
    if (!ReadPrefixes(&chunk_data, &remaining, chunk_host.entry, prefix_count))
      return false;
    DCHECK_GE(remaining, 0);
    hosts->push_back(chunk_host);
  } else {
    SBPrefix host;
    const int min_size = 2 * sizeof(SBPrefix) + 1;
    while (remaining >= min_size) {
      if (!ReadHostAndPrefixCount(&chunk_data, &remaining,
                                  &host, &prefix_count)) {
        return false;
      }
      DCHECK_GE(remaining, 0);
      SBChunkHost chunk_host;
      chunk_host.host = host;
      chunk_host.entry = SBEntry::Create(type, prefix_count);
      hosts->push_back(chunk_host);
      if (prefix_count == 0) {
        // There is only an add chunk number (no prefixes).
        int chunk_id;
        if (!ReadChunkId(&chunk_data, &remaining, &chunk_id))
          return false;
        DCHECK_GE(remaining, 0);
        chunk_host.entry->set_chunk_id(chunk_id);
        continue;
      }
      if (!ReadPrefixes(&chunk_data, &remaining, chunk_host.entry,
                        prefix_count))
        return false;
      DCHECK_GE(remaining, 0);
    }
  }
  return remaining == 0;
}

bool SafeBrowsingProtocolParser::ReadHostAndPrefixCount(
    const char** data, int* remaining, SBPrefix* host, int* count) {
  if (static_cast<size_t>(*remaining) < sizeof(SBPrefix) + 1)
    return false;
  // Next 4 bytes are the host prefix.
  memcpy(host, *data, sizeof(SBPrefix));
  *data += sizeof(SBPrefix);
  *remaining -= sizeof(SBPrefix);

  // Next 1 byte is the prefix count (could be zero, but never negative).
  *count = static_cast<unsigned char>(**data);
  *data += 1;
  *remaining -= 1;
  DCHECK_GE(*remaining, 0);
  return true;
}

bool SafeBrowsingProtocolParser::ReadChunkId(
    const char** data, int* remaining, int* chunk_id) {
  // Protocol says four bytes, not sizeof(int).  Make sure those
  // values are the same.
  DCHECK_EQ(sizeof(*chunk_id), 4u);
  if (static_cast<size_t>(*remaining) < sizeof(*chunk_id))
    return false;
  memcpy(chunk_id, *data, sizeof(*chunk_id));
  *data += sizeof(*chunk_id);
  *remaining -= sizeof(*chunk_id);
  *chunk_id = base::HostToNet32(*chunk_id);
  DCHECK_GE(*remaining, 0);
  return true;
}

bool SafeBrowsingProtocolParser::ReadPrefixes(
    const char** data, int* remaining, SBEntry* entry, int count) {
  int hash_len = entry->HashLen();
  for (int i = 0; i < count; ++i) {
    if (entry->IsSub()) {
      int chunk_id;
      if (!ReadChunkId(data, remaining, &chunk_id))
        return false;
      DCHECK_GE(*remaining, 0);
      entry->SetChunkIdAtPrefix(i, chunk_id);
    }

    if (*remaining < hash_len)
      return false;
    if (entry->IsPrefix()) {
      DCHECK_EQ(hash_len, (int)sizeof(SBPrefix));
      entry->SetPrefixAt(i, *reinterpret_cast<const SBPrefix*>(*data));
    } else {
      DCHECK_EQ(hash_len, (int)sizeof(SBFullHash));
      entry->SetFullHashAt(i, *reinterpret_cast<const SBFullHash*>(*data));
    }
    *data += hash_len;
    *remaining -= hash_len;
    DCHECK_GE(*remaining, 0);
  }

  return true;
}
