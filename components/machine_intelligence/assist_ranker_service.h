// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MACHINE_INTELLIGENCE_ASSIST_RANKER_SERVICE_H_
#define COMPONENTS_MACHINE_INTELLIGENCE_ASSIST_RANKER_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace machine_intelligence {

class BinaryClassifierPredictor;

// TODO(crbug.com/778468) : Refactor this so that the service owns the predictor
// objects and enforce model uniqueness through internal registration in order
// to avoid cache collisions.
//
// Service that provides Predictor objects.
class AssistRankerService : public KeyedService {
 public:
  AssistRankerService() = default;

  // Returns a binary classification model. |model_filename| is the filename of
  // the cached model. It should be unique to this predictor to avoid cache
  // collision. |model_url| represents a unique ID for the desired model (see
  // ranker_model_loader.h for more details). |uma_prefix| is used to log
  // histograms related to the loading of the model.
  virtual std::unique_ptr<BinaryClassifierPredictor>
  FetchBinaryClassifierPredictor(GURL model_url,
                                 const std::string& model_filename,
                                 const std::string& uma_prefix) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistRankerService);
};

}  // namespace machine_intelligence

#endif  // COMPONENTS_MACHINE_INTELLIGENCE_ASSIST_RANKER_SERVICE_H_
