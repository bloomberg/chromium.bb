// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_io.h"

#include <limits>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_store.h"

namespace {

const char header[] = "[Chromium::PrivacyBlacklist]";
const char name_tag[] = "Name:";
const char url_tag[] = "URL:";
const char arrow_tag[] = "=>";
const char eol[] = "\n\r";

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
  return cur + size <= end && std::equal(tag, tag + size - 1, cur);
}

}  // namespace

// static
bool BlacklistIO::ReadText(Blacklist* blacklist,
                           const FilePath& path,
                           std::string* error_string) {
  DCHECK(blacklist);

  // Memory map for efficient parsing. If the file cannot fit in available
  // memory it would be the least of our worries. Typical blacklist files
  // are less than 200K.
  file_util::MemoryMappedFile input;
  if (!input.Initialize(path) || !input.data()) {
    *error_string = "File I/O error. Check path and permissions.";
    return false;
  }

  const char* cur = reinterpret_cast<const char*>(input.data());
  const char* end = cur + input.length();

  // Check header.
  if (!StartsWith(cur, end, header, arraysize(header))) {
    *error_string = "Incorrect header.";
    return false;
  }

  Blacklist::EntryList entries;
  Blacklist::ProviderList providers;

  Blacklist::Provider* provider = new Blacklist::Provider;
  providers.push_back(linked_ptr<Blacklist::Provider>(provider));

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
        cur = std::find_if(cur + arraysize(name_tag), end, IsNotWhiteSpace());
        const char* skip = std::find_if(cur, end, IsWhiteSpace());
        if (skip < end)
          provider->set_name(std::string(cur, skip));
      } else if (StartsWith(cur, end, url_tag, arraysize(url_tag))) {
        cur = std::find_if(cur + arraysize(url_tag), end, IsNotWhiteSpace());
        const char* skip = std::find_if(cur, end, IsWhiteSpace());
        if (skip < end)
          provider->set_url(std::string(cur, skip));
      }
      cur = std::find(cur, end, '\n') + 1;
      continue;
    }

    const char* skip = std::find_if(cur, end, IsWhiteSpace());
    std::string pattern(cur, skip);

    cur = std::find_if(cur + pattern.size(), end, IsNotWhiteSpace());
    if (!StartsWith(cur, end, arrow_tag, arraysize(arrow_tag))) {
      *error_string = "Missing => in rule.";
      return false;
    }

    linked_ptr<Blacklist::Entry> entry(new Blacklist::Entry(pattern, provider));

    cur = std::find_if(cur + arraysize(arrow_tag), end, IsNotWhiteSpace());
    skip = std::find_first_of(cur, end, eol, eol + 2);
    std::string buf(cur, skip);
    cur = skip + 1;

    StringTokenizer tokenizer(buf, " (),\n\r");
    tokenizer.set_options(StringTokenizer::RETURN_DELIMS);

    bool in_attribute = false;
    unsigned int last_attribute = 0;

    while (tokenizer.GetNext()) {
      if (tokenizer.token_is_delim()) {
        switch (*tokenizer.token_begin()) {
          case '(':
            if (in_attribute) {
              *error_string = "Unexpected ( in attribute parameters.";
              return false;
            }
            in_attribute = true;
            continue;
          case ')':
            if (!in_attribute) {
              *error_string = "Unexpected ) in attribute list.";
              return false;
            }
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
          entry->AddType(tokenizer.token());
      } else {
        // Filter attribute. Unrecognized attributes are ignored.
        last_attribute = Blacklist::String2Attribute(tokenizer.token());
        entry->AddAttributes(last_attribute);
      }
    }
    entries.push_back(entry);
  }

  for (Blacklist::EntryList::iterator i = entries.begin();
       i != entries.end(); ++i) {
    blacklist->AddEntry(i->release());
  }
  for (Blacklist::ProviderList::iterator i = providers.begin();
       i != providers.end(); ++i) {
    blacklist->AddProvider(i->release());
  }

  return true;
}

// static
bool BlacklistIO::ReadBinary(Blacklist* blacklist, const FilePath& path) {
  DCHECK(blacklist);

  FILE* fp = file_util::OpenFile(path, "rb");
  if (fp == NULL)
    return false;

  BlacklistStoreInput input(fp);

  // Read the providers.
  uint32 num_providers = input.ReadNumProviders();
  if (num_providers == std::numeric_limits<uint32>::max())
    return false;

  Blacklist::EntryList entries;
  Blacklist::ProviderList providers;
  std::map<size_t, Blacklist::Provider*> provider_map;

  std::string name;
  std::string url;
  for (size_t i = 0; i < num_providers; ++i) {
    if (!input.ReadProvider(&name, &url))
      return false;
    provider_map[i] = new Blacklist::Provider(name.c_str(), url.c_str());
    providers.push_back(linked_ptr<Blacklist::Provider>(provider_map[i]));
  }

  // Read the entries.
  uint32 num_entries = input.ReadNumEntries();
  if (num_entries == std::numeric_limits<uint32>::max())
    return false;

  std::string pattern;
  unsigned int attributes, provider;
  std::vector<std::string> types;
  for (size_t i = 0; i < num_entries; ++i) {
    if (!input.ReadEntry(&pattern, &attributes, &types, &provider))
      return false;

    Blacklist::Entry* entry =
        new Blacklist::Entry(pattern, provider_map[provider]);
    entry->AddAttributes(attributes);
    entry->SwapTypes(&types);
    entries.push_back(linked_ptr<Blacklist::Entry>(entry));
  }

  for (Blacklist::EntryList::iterator i = entries.begin();
       i != entries.end(); ++i) {
    blacklist->AddEntry(i->release());
  }
  for (Blacklist::ProviderList::iterator i = providers.begin();
       i != providers.end(); ++i) {
    blacklist->AddProvider(i->release());
  }

  return true;
}

// static
bool BlacklistIO::WriteBinary(const Blacklist* blacklist,
                              const FilePath& file) {
  BlacklistStoreOutput output(file_util::OpenFile(file, "wb"));
  if (!output.is_good())
    return false;

  Blacklist::EntryList entries(blacklist->entries_begin(),
                               blacklist->entries_end());
  Blacklist::ProviderList providers(blacklist->providers_begin(),
                                    blacklist->providers_end());

  // Output providers, give each one an index.
  std::map<const Blacklist::Provider*, uint32> index;
  uint32 current = 0;
  if (!output.ReserveProviders(providers.size()))
    return false;

  for (Blacklist::ProviderList::const_iterator i = providers.begin();
       i != providers.end(); ++i, ++current) {
    if (!output.StoreProvider((*i)->name(), (*i)->url()))
      return false;
    index[i->get()] = current;
  }

  // Output entries, replacing the provider with its index.
  if (!output.ReserveEntries(entries.size()))
    return false;

  for (Blacklist::EntryList::const_iterator i = entries.begin();
       i != entries.end(); ++i) {
    if (!output.StoreEntry((*i)->pattern_,
                           (*i)->attributes_,
                           (*i)->types_,
                           index[(*i)->provider_])) {
      return false;
    }
  }
  return true;
}
