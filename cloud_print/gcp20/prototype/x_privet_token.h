// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_X_PRIVET_TOKEN_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_X_PRIVET_TOKEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"

// Class for generating and checking X-Privet-Token.
class XPrivetToken {
 public:
  // Initializes the object.
  XPrivetToken();

  // Destroys the object.
  ~XPrivetToken() {}

  // Generates X-Privet-Token for /privet/info request. Updates secret
  // if expired.
  std::string GenerateXToken();

  // Checks
  bool CheckValidXToken(const std::string& token) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(XPrivetTokenTest, Generation);
  FRIEND_TEST_ALL_PREFIXES(XPrivetTokenTest, CheckingValid);
  FRIEND_TEST_ALL_PREFIXES(XPrivetTokenTest, CheckingInvalid);

  // For testing purposes.
  XPrivetToken(const std::string& secret, const base::Time& gen_time);

  // Generates X-Privet-Token for with certain time of issue.
  std::string GenerateXTokenWithTime(uint64 issue_time) const;

  // Creates new XPrivetToken secret.
  void UpdateSecret();

  // X-Privet-Token secret.
  std::string secret_;

  // Time of last secret generation.
  base::Time last_gen_time_;
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_X_PRIVET_TOKEN_H_

