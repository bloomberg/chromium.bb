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

  // Returns the value we use for the "os=" parameter. Possible return values
  // include: "mac", "win", "android", "cros", "linux", and "openbsd".
  static const char* getOS();

  // Returns the value we use for the "arch=" parameter. Possible return values
  // include: "x86", "x64", and "arm".
  static const char* getArch();

  // Returns the value we use for the "nacl_arch" parameter. Note that this may
  // be different from the "arch" parameter above (e.g. one may be 32-bit and
  // the other 64-bit). Possible return values include: "x86-32", "x86-64",
  // "arm", and "mips32".
  static const char* getNaclArch();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(OmahaQueryParams);
};

}  // namespace chrome

#endif  // CHROME_COMMON_OMAHA_QUERY_PARAMS_H_
