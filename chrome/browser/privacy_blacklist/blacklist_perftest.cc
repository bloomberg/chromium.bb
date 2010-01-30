// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/perftimer.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Number of URLs to try for each test.
const size_t kNumUrls = 10000;

// Fixed random seed for srand(), so always the same random blacklist is
// generated.
const unsigned int kSeed = 0;

// Number of entries for the benchmark blacklist. The blacklist will contain
// five different kinds of patterns:
//   - host.name.tld/
//   - /some/path
//   - /some/path/script?
//   - host.name.tld/some/path
//   - host.name.tld/some/path/?script
const size_t kNumHostEntries = 500;
const size_t kNumPathEntries = 500;
const size_t kNumScriptEntries = 500;
const size_t kNumHostPathEntries = 250;
const size_t kNumHostScriptEntries = 250;
const size_t kNumPattern = kNumHostEntries + kNumPathEntries +
                           kNumScriptEntries + kNumHostPathEntries +
                           kNumHostScriptEntries;

// Returns a random integer between 0 and |max| - 1.
static size_t RandomIndex(size_t max) {
  DCHECK_GT(max, 0U);

  return static_cast<size_t>(static_cast<float>(max) *
                             (rand() / (RAND_MAX + 1.0)));
}

// Generates a random alpha-numeric string of a given |length|.
static std::string GetRandomString(size_t length) {
  // Alpha-numeric characters without the letter a. This is later used
  // in |GetNotMatchingURL| to generate disjoint patterns.
  static const char chars[] = "bcdefghijklmnopqrstuvwxyz0123456789";
  std::string result;

  for (size_t i = 0; i < length; ++i) {
    result += chars[RandomIndex(arraysize(chars) - 1)];
  }

  return result;
}

// Generates a URL that will match a given pattern. Assumes that the pattern
// consists of at least a leading wildcard and a trailing wildcard. If invert
// is true, the returned URL will NOT match the pattern or any other pattern
// generated for the blacklist.
static std::string GetURL(const std::string& pattern, bool invert) {
  DCHECK_GT(pattern.length(), 2U);
  DCHECK_EQ(pattern[0], '@');
  DCHECK_EQ(pattern[pattern.length() - 1], '@');

  std::string url("http://");

  // If this is a path pattern, prepend host.tld, otherwise just prepend host.
  if (pattern[1] == '/') {
    url += GetRandomString(8) + "." + GetRandomString(3);
  } else {
    url += GetRandomString(6) + ".";
  }

  // Add the constant part of the pattern.
  url += pattern.substr(1, pattern.length() - 2);

  // If this is a script pattern, append name1=value parameters, otherwise
  // append a /path/file.ext
  if (pattern[pattern.length() - 2] == '?') {
    url += GetRandomString(5) + "=" + GetRandomString(10);
  } else {
    url += GetRandomString(6) + "/" + GetRandomString(6) + "." +
           GetRandomString(3);
  }

  // If the URL is supposed to match no pattern, we introduce the letter 'a'
  // which is not used for generating patterns
  if (invert) {
    // Position of the host/path separating slash. Skipping the 7 characters
    // of "http://".
    size_t host_path_separator = 7 + url.find_first_of('/', 7);

    // If the pattern is starting with a /path, modify the first path element,
    // otherwise modify the hostname.
    if (pattern[1] == '/') {
      url[host_path_separator + 3] = 'a';
    } else {
      // Locate the domain name.
      url[url.substr(0, host_path_separator).find_last_of('.') - 2] = 'a';
    }
  }

  return url;
}

class BlacklistPerfTest : public testing::Test {
 protected:
  virtual void SetUp() {
    srand(kSeed);

    // Generate random benchmark blacklist.
    Blacklist::Provider* provider = new Blacklist::Provider("test",
                                                            "http://test.com",
                                                            L"test");
    blacklist_->AddProvider(provider);

    // Create host.tld/ patterns.
    for (size_t i = 0; i < kNumHostEntries; ++i) {
      std::string pattern = "@" + GetRandomString(8) + "." +
                            GetRandomString(3) + "/@";
      blacklist_->AddEntry(new Blacklist::Entry(pattern, provider, false));
    }

    // Create /some/path/ patterns.
    for (size_t i = 0; i < kNumPathEntries; ++i) {
      std::string pattern = "@/" + GetRandomString(6) + "/" +
                            GetRandomString(6) + "/@";
      blacklist_->AddEntry(new Blacklist::Entry(pattern, provider, false));
    }

    // Create /some/path/script? patterns.
    for (size_t i = 0; i < kNumScriptEntries; ++i) {
      std::string pattern = "@/" + GetRandomString(6) + "/" +
                            GetRandomString(6) + "/" + GetRandomString(6) +
                            "?@";
      blacklist_->AddEntry(new Blacklist::Entry(pattern, provider, false));
    }

    // Create host.tld/some/path patterns.
    for (size_t i = 0; i < kNumHostPathEntries; ++i) {
      std::string pattern = "@" + GetRandomString(8) + "." +
                            GetRandomString(3) + "/" + GetRandomString(6) +
                            "/" + GetRandomString(6) + "/@";
      blacklist_->AddEntry(new Blacklist::Entry(pattern, provider, false));
    }

    // Create host.tld/some/path/script? patterns.
    for (size_t i = 0; i < kNumHostScriptEntries; ++i) {
      std::string pattern = "@" + GetRandomString(8) + "." +
                            GetRandomString(3) + "/" + GetRandomString(6) +
                            "/" + GetRandomString(6) + "?@";
      blacklist_->AddEntry(new Blacklist::Entry(pattern, provider, false));
    }

    DCHECK_EQ(std::distance(blacklist_->entries_begin(),
              blacklist_->entries_end()), static_cast<ptrdiff_t>(kNumPattern));
  }

  // Randomly generated benchmark blacklist.
  scoped_refptr<Blacklist> blacklist_;
};

// Perf test for matching URLs which are contained in the blacklist.
TEST_F(BlacklistPerfTest, Match) {
  // Pick random patterns and generate matching URLs
  std::vector<std::string> urls;
  for (size_t i = 0; i < kNumUrls; ++i) {
    const Blacklist::Entry* entry = (blacklist_->entries_begin() +
                                     RandomIndex(kNumPattern))->get();
    urls.push_back(GetURL(entry->pattern(), false));
  }

  // Measure performance for matching urls against the blacklist.
  PerfTimeLogger timer("blacklist_match");

  for (size_t i = 0; i < kNumUrls; ++i) {
    scoped_ptr<Blacklist::Match> match(blacklist_->FindMatch(GURL(urls[i])));
    ASSERT_TRUE(match.get());
  }

  timer.Done();
}

// Perf test for matching URLs which are NOT contained in the blacklist.
TEST_F(BlacklistPerfTest, Mismatch) {
  // We still use randomly picked patterns for generating URLs, but the URLs
  // are made so that they do NOT match any generated pattern
  std::vector<std::string> urls;
  for (size_t i = 0; i < kNumUrls; ++i) {
    const Blacklist::Entry* entry = (blacklist_->entries_begin() +
                                     RandomIndex(kNumPattern))->get();
    urls.push_back(GetURL(entry->pattern(), true));
  }

  // Measure performance for matching urls against the blacklist.
  PerfTimeLogger timer("blacklist_mismatch");

  for (size_t i = 0; i < kNumUrls; ++i) {
    scoped_ptr<Blacklist::Match> match(blacklist_->FindMatch(GURL(urls[i])));
    ASSERT_FALSE(match.get());
  }

  timer.Done();
}

}  // namespace
