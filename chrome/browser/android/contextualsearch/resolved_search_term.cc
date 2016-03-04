// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/resolved_search_term.h"

#include "net/url_request/url_fetcher.h"

ResolvedSearchTerm::ResolvedSearchTerm(int response_code)
    : is_invalid(response_code == net::URLFetcher::RESPONSE_CODE_INVALID),
      response_code(response_code),
      search_term(""),
      display_text(""),
      alternate_term(""),
      prevent_preload(false),
      selection_start_adjust(0),
      selection_end_adjust(0),
      context_language("") {}

ResolvedSearchTerm::ResolvedSearchTerm(bool is_invalid,
                                       int response_code,
                                       const std::string& search_term,
                                       const std::string& display_text,
                                       const std::string& alternate_term,
                                       bool prevent_preload,
                                       int selection_start_adjust,
                                       int selection_end_adjust,
                                       const std::string& context_language)
    : is_invalid(is_invalid),
      response_code(response_code),
      search_term(search_term),
      display_text(display_text),
      alternate_term(alternate_term),
      prevent_preload(prevent_preload),
      selection_start_adjust(selection_start_adjust),
      selection_end_adjust(selection_end_adjust),
      context_language(context_language) {}

ResolvedSearchTerm::~ResolvedSearchTerm() {}
