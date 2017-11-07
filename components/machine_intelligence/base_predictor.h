// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MACHINE_INTELLIGENCE_BASE_PREDICTOR_H_
#define COMPONENTS_MACHINE_INTELLIGENCE_BASE_PREDICTOR_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "components/machine_intelligence/ranker_model_loader.h"

namespace machine_intelligence {

class RankerModel;

// Predictors are objects that provide an interface for prediction, as well as
// encapsulate the logic for loading the model. Sub-classes of BasePredictor
// implement an interface that depends on the nature of the suported model.
// Subclasses of BasePredictor will also need to implement an Initialize method
// that will be called once the model is available, and a static validation
// function with the following signature:
//
// static RankerModelStatus ValidateModel(const RankerModel& model);
class BasePredictor {
 public:
  BasePredictor();
  virtual ~BasePredictor();

  bool IsReady();

 protected:
  // The model used for prediction.
  std::unique_ptr<RankerModel> ranker_model_;

  // Called when the RankerModelLoader has finished loading the model. Returns
  // true only if the model was succesfully loaded and is ready to predict.
  virtual bool Initialize() = 0;

  // Loads a model and make it available for prediction.
  void LoadModel(std::unique_ptr<RankerModelLoader> model_loader);
  // Called once the model loader as succesfully loaded the model.
  void OnModelAvailable(std::unique_ptr<RankerModel> model);
  std::unique_ptr<RankerModelLoader> model_loader_;

 private:
  bool is_ready_ = false;

  DISALLOW_COPY_AND_ASSIGN(BasePredictor);
};

}  // namespace machine_intelligence
#endif  // COMPONENTS_MACHINE_INTELLIGENCE_BASE_PREDICTOR_H_
