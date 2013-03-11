// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_OMAHA_QUERY_PARAMS_H_
#define CHROME_COMMON_OMAHA_QUERY_PARAMS_H_

#include <string>

#include "base/basictypes.h"

namespace chrome {

class OmahaQueryParams {
 public:
  enum ProdId {
    CHROME = 0,
    CHROMECRX,
    CHROMIUMCRX,
  };

  // Generates a string of URL query paramaters to be used when getting
  // component and extension updates. Includes the following fields: os, arch,
  // prod, prodchannel, prodversion.
  static std::string Get(ProdId prod);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(OmahaQueryParams);
};

}  // namespace chrome

#endif  // CHROME_COMMON_OMAHA_QUERY_PARAMS_H_
