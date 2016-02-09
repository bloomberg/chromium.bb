// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_HEADER_H_
#define COMPONENTS_DOMAIN_RELIABILITY_HEADER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "url/gurl.h"

namespace domain_reliability {

struct DomainReliabilityConfig;

class DOMAIN_RELIABILITY_EXPORT DomainReliabilityHeader {
 public:
  // The outcome of the parse: PARSE_SET_CONFIG if the header specified a
  // valid config to set, PARSE_CLEAR_CONFIG if the header requested that an
  // existing config (if any) be cleared, or PARSE_ERROR if the heder did not
  // parse correctly.
  enum ParseStatus {
    PARSE_SET_CONFIG,
    PARSE_CLEAR_CONFIG,
    PARSE_ERROR
  };

  ~DomainReliabilityHeader();

  static scoped_ptr<DomainReliabilityHeader> Parse(base::StringPiece value);

  bool IsSetConfig() const { return status_ == PARSE_SET_CONFIG; }
  bool IsClearConfig() const { return status_ == PARSE_CLEAR_CONFIG; }
  bool IsParseError() const { return status_ == PARSE_ERROR; }

  ParseStatus status() const { return status_; }
  const DomainReliabilityConfig& config() const;
  base::TimeDelta max_age() const;

  scoped_ptr<DomainReliabilityConfig> ReleaseConfig();

  std::string ToString() const;

 private:
  // Constructor for PARSE_SET_CONFIG status.
  DomainReliabilityHeader(ParseStatus status,
                          scoped_ptr<DomainReliabilityConfig> config,
                          base::TimeDelta max_age);
  // Constructor for PARSE_CLEAR_CONFIG and PARSE_ERROR statuses.
  DomainReliabilityHeader(ParseStatus status);

  ParseStatus status_;

  // The configuration specified by the header, if the status is
  // PARSE_SET_CONFIG.
  scoped_ptr<DomainReliabilityConfig> config_;

  // The max-age specified by the header, if the status is PARSE_SET_CONFIG.
  base::TimeDelta max_age_;

  DISALLOW_COPY_AND_ASSIGN(DomainReliabilityHeader);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_HEADER_H_
