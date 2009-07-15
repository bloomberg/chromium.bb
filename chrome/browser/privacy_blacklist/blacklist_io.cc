// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_io.h"

#include <algorithm>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/string_tokenizer.h"
#include "chrome/browser/privacy_blacklist/blacklist_store.h"

namespace {

const char header[] = "[Chromium::PrivacyBlacklist]";
const char name_tag[] = "Name:";
const char url_tag[] = "URL:";
const char arrow_tag[] = "=>";

class IsWhiteSpace {
 public:
  bool operator()(const char& c) const {
    return IsAsciiWhitespace(c);
  }
};

class IsNotWhiteSpace {
 public:
  bool operator()(const char& c) const {
    return !IsAsciiWhitespace(c);
  }
};

bool StartsWith(const char* cur, const char* end,
                const char* tag, std::size_t size) {
  return cur+size <= end && std::equal(tag, tag+size-1, cur);
}

}  // namespace

BlacklistIO::BlacklistIO() {}

BlacklistIO::~BlacklistIO() {
  for (std::list<Blacklist::Entry*>::iterator i = blacklist_.begin();
       i != blacklist_.end(); ++i) {
    delete *i;
  }
  for (std::list<Blacklist::Provider*>::iterator i = providers_.begin();
       i != providers_.end(); ++i) {
    delete *i;
  }
}

bool BlacklistIO::Read(const FilePath& file) {
  // Memory map for efficient parsing. If the file cannot fit in available
  // memory it would be the least of our worries. Typical blacklist files
  // are less than 200K.
  file_util::MemoryMappedFile input;
  if (!input.Initialize(file) || !input.data())
    return false;

  const char* cur = reinterpret_cast<const char*>(input.data());
  const char* end = cur + input.length();

  // Check header.
  if (!StartsWith(cur, end, header, arraysize(header)))
    return false;

  Blacklist::Provider* provider = new Blacklist::Provider;
  providers_.push_back(provider);

  cur = std::find(cur, end, '\n') + 1;  // Skip past EOL.

  // Each loop iteration takes care of one input line.
  while (cur < end) {
    // Skip whitespace at beginning of line.
    cur = std::find_if(cur, end, IsNotWhiteSpace());
    if (cur == end)
      break;

    if (*cur == '#') {
      cur = std::find(cur, end, '\n') + 1;
      continue;
    }

    if (*cur == '|') {
      ++cur;
      if (StartsWith(cur, end, name_tag, arraysize(name_tag))) {
        // Edge condition: if the find below fails, the next one will too,
        // so we'll just skip to the EOF below.
        cur = std::find_if(cur+arraysize(name_tag), end, IsNotWhiteSpace());
        const char* skip = std::find_if(cur, end, IsWhiteSpace());
        if (skip < end)
          provider->set_name(std::string(cur, skip));
      } else if (StartsWith(cur, end, url_tag, arraysize(url_tag))) {
        cur = std::find_if(cur+arraysize(url_tag), end, IsNotWhiteSpace());
        const char* skip = std::find_if(cur, end, IsWhiteSpace());
        if (skip < end)
          provider->set_url(std::string(cur, skip));
      }
      cur = std::find(cur, end, '\n') + 1;
      continue;
    }

    const char* skip = std::find_if(cur, end, IsWhiteSpace());
    std::string pattern(cur, skip);

    cur = std::find_if(cur+pattern.size(), end, IsNotWhiteSpace());
    if (!StartsWith(cur, end, arrow_tag, arraysize(arrow_tag)))
      return false;

    scoped_ptr<Blacklist::Entry> entry(new Blacklist::Entry(pattern, provider));

    cur = std::find_if(cur+arraysize(arrow_tag), end, IsNotWhiteSpace());
    skip = std::find(cur, end, '\n');
    std::string buf(cur, skip);
    cur = skip + 1;

    StringTokenizer tokenier(buf, " (),");
    tokenier.set_options(StringTokenizer::RETURN_DELIMS);

    bool in_attribute = false;
    unsigned int last_attribute = 0;

    while (tokenier.GetNext()) {
      if (tokenier.token_is_delim()) {
        switch (*tokenier.token_begin()) {
          case '(':
            if (in_attribute) return false;
            in_attribute = true;
            continue;
          case ')':
            if (!in_attribute) return false;
            in_attribute = false;
            continue;
          default:
            // No state change for other delimiters.
            continue;
        }
      }

      if (in_attribute) {
        // The only attribute to support sub_tokens is kBlockByType, for now.
        if (last_attribute == Blacklist::kBlockByType)
          entry->AddType(tokenier.token());
      } else {
        // Filter attribute. Unrecognized attributes are ignored.
        last_attribute = Blacklist::String2Attribute(tokenier.token());
        entry->AddAttributes(last_attribute);
      }
    }
    blacklist_.push_back(entry.release());
  }
  return true;
}

bool BlacklistIO::Write(const FilePath& file) {
  BlacklistStoreOutput output(file_util::OpenFile(file, "wb"));

  // Output providers, give each one an index.
  std::map<const Blacklist::Provider*, uint32> index;
  uint32 current = 0;
  output.ReserveProviders(providers_.size());
  for (std::list<Blacklist::Provider*>::const_iterator i = providers_.begin();
       i != providers_.end(); ++i, ++current) {
    output.StoreProvider((*i)->name(), (*i)->url());
    index[*i] = current;
  }

  // Output entries, replacing the provider with its index.
  output.ReserveEntries(blacklist_.size());
  for (std::list<Blacklist::Entry*>::const_iterator i = blacklist_.begin();
       i != blacklist_.end(); ++i) {
    output.StoreEntry((*i)->pattern_,
                      (*i)->attributes_,
                      (*i)->types_,
                      index[(*i)->provider_]);
  }
  return true;
}
