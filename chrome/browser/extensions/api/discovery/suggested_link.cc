// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/discovery/suggested_link.h"

namespace extensions {

SuggestedLink::SuggestedLink(const std::string& link_url,
                             const std::string& link_text,
                             double score)
    : link_url_(link_url),
      link_text_(link_text),
      score_(score) {}

SuggestedLink::~SuggestedLink() {}

}  // namespace extensions
