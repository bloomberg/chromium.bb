// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_FAKE_RANKER_MODEL_LOADER_H_
#define COMPONENTS_TRANSLATE_FAKE_RANKER_MODEL_LOADER_H_

#include <memory>

#include "components/machine_intelligence/proto/ranker_model.pb.h"
#include "components/machine_intelligence/ranker_model_loader.h"

namespace machine_intelligence {

namespace testing {

// Simplified RankerModelLoader for testing.
class FakeRankerModelLoader : public RankerModelLoader {
 public:
  FakeRankerModelLoader(ValidateModelCallback validate_model_cb,
                        OnModelAvailableCallback on_model_available_cb,
                        std::unique_ptr<RankerModel> ranker_model);
  ~FakeRankerModelLoader() override;

  void NotifyOfRankerActivity() override;

 private:
  std::unique_ptr<RankerModel> ranker_model_;
  const ValidateModelCallback validate_model_cb_;
  const OnModelAvailableCallback on_model_available_cb_;
  DISALLOW_COPY_AND_ASSIGN(FakeRankerModelLoader);
};

}  // namespace testing

}  // namespace machine_intelligence

#endif  // COMPONENTS_MACHINE_INTELLIGENCE_FAKE_RANKER_MODEL_LOADER_H_
