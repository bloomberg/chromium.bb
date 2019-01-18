// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_HTTP_STRUCTURED_HEADER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_HTTP_STRUCTURED_HEADER_H_

#include <map>
#include <vector>
#include "base/optional.h"
#include "base/strings/string_piece.h"

namespace content {
namespace http_structured_header {

struct ParameterisedIdentifier {
  std::string identifier;
  std::map<std::string, std::string> params;

  ParameterisedIdentifier();
  ParameterisedIdentifier(const ParameterisedIdentifier&);
  ~ParameterisedIdentifier();
};

typedef std::vector<ParameterisedIdentifier> ParameterisedList;

base::Optional<ParameterisedList> ParseParameterisedList(
    const base::StringPiece& str);

}  // namespace http_structured_header
}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_HTTP_STRUCTURED_HEADER_H_
