// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_API_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_API_FEATURE_H_

#include <string>

#include "extensions/common/features/simple_feature.h"

namespace extensions {

class APIFeature : public SimpleFeature {
 public:
  APIFeature();
  virtual ~APIFeature();

  // extensions::Feature:
  virtual bool IsInternal() const OVERRIDE;
  virtual bool IsBlockedInServiceWorker() const OVERRIDE;

  virtual std::string Parse(const base::DictionaryValue* value) OVERRIDE;

 private:
  bool internal_;
  bool blocked_in_service_worker_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_API_FEATURE_H_
