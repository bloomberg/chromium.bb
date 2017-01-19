// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_CONTEXT_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_CONTEXT_H_

#include <string>

#include "base/macros.h"
#include "url/gurl.h"

// Encapsulates key parts of a Contextual Search Context, including surrounding
// text.
struct ContextualSearchContext {
 public:
  ContextualSearchContext(const std::string& selected_text,
                          const std::string& home_country,
                          const GURL& page_url,
                          const std::string& encoding);
  ~ContextualSearchContext();

  const std::string selected_text;
  const std::string home_country;
  const GURL page_url;
  const std::string encoding;

  base::string16 surrounding_text;
  int start_offset;
  int end_offset;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchContext);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_CONTEXT_H_
