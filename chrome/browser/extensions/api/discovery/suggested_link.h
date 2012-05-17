// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_SUGGESTED_LINK_H_
#define CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_SUGGESTED_LINK_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"

namespace extensions {

// ExtensionSuggestedLinks contains a list of scored links that the extension
// wants to inject in the NTP's recommended pane.
class SuggestedLink {
 public:
  SuggestedLink(const std::string& link_url_, const std::string& link_text_,
                double score);
  ~SuggestedLink();

  const std::string& link_url() const { return link_url_; }
  const std::string& link_text() const { return link_text_; }
  double score() const { return score_; }

 private:
  std::string link_url_;
  std::string link_text_;

  // |score_| is a value between 0 and 1 indicating the relative importance of
  // this suggested link. A link with score 1 is twice as likely to be presented
  // than one with score 0.5. Use a score of 1 if no information is available on
  // the relative importance of the links.
  double score_;

  DISALLOW_COPY_AND_ASSIGN(SuggestedLink);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DISCOVERY_SUGGESTED_LINK_H_
