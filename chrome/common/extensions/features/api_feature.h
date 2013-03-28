// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURES_API_FEATURE_H_
#define CHROME_COMMON_EXTENSIONS_FEATURES_API_FEATURE_H_

#include "chrome/common/extensions/features/simple_feature.h"

namespace extensions {

class APIFeature : public SimpleFeature {
 public:
  APIFeature();
  virtual ~APIFeature();

  virtual std::string Parse(const DictionaryValue* value) OVERRIDE;
};

}  // extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURES_API_FEATURE_H_
