// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_URL_PREFIX_H_
#define CHROME_BROWSER_AUTOCOMPLETE_URL_PREFIX_H_
#pragma once

#include <vector>

#include "base/string16.h"

struct URLPrefix;
typedef std::vector<URLPrefix> URLPrefixes;

// A URL prefix; combinations of schemes and (least significant) domain labels
// that may be inferred from certain URL-like input strings.
struct URLPrefix {
  URLPrefix(const string16& prefix, size_t num_components);

  // Returns a vector of URL prefixes sorted by descending number of components.
  static const URLPrefixes& GetURLPrefixes();

  // Returns if the argument is a valid URL prefix.
  static bool IsURLPrefix(const string16& prefix);

  // Returns the prefix with the most components that begins |text|, or NULL.
  // |prefix_suffix| (which may be empty) is appended to every attempted prefix,
  // which is useful for finding the innermost match of user input in a URL.
  // Performs case insensitive string comparison.
  static const URLPrefix* BestURLPrefix(const string16& text,
                                        const string16& prefix_suffix);

  string16 prefix;

  // The number of URL components (scheme, domain label, etc.) in the prefix.
  // For example, "http://foo.com" and "www.bar.com" each have one component,
  // while "ftp://ftp.ftp.com" has two, and "mysite.com" has none.
  size_t num_components;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_URL_PREFIX_H_
