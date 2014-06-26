// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMAHA_QUERY_PARAMS_OMAHA_QUERY_PARAMS_DELEGATE_H_
#define COMPONENTS_OMAHA_QUERY_PARAMS_OMAHA_QUERY_PARAMS_DELEGATE_H_

#include <string>

#include "base/basictypes.h"

namespace omaha_query_params {

// Embedders can specify an OmahaQueryParamsDelegate to provide additional
// custom parameters. If not specified (Set is never called), no additional
// parameters are added.
class OmahaQueryParamsDelegate {
 public:
  OmahaQueryParamsDelegate();
  virtual ~OmahaQueryParamsDelegate();

  // Returns additional parameters, if any. If there are any parameters, the
  // string should begin with a & character.
  virtual std::string GetExtraParams() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(OmahaQueryParamsDelegate);
};

}  // namespace omaha_query_params

#endif  // COMPONENTS_OMAHA_QUERY_PARAMS_OMAHA_QUERY_PARAMS_DELEGATE_H_
