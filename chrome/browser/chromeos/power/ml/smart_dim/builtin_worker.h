// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_BUILTIN_WORKER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_BUILTIN_WORKER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/smart_dim_worker.h"
#include "chromeos/services/machine_learning/public/mojom/graph_executor.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace power {
namespace ml {

// SmartDimWorker that uses a built-in preprocessor config and ML service model
// file.
class BuiltinWorker : public SmartDimWorker {
 public:
  BuiltinWorker();
  ~BuiltinWorker() override;

  // SmartDimWorker overrides:
  const assist_ranker::ExamplePreprocessorConfig* GetPreprocessorConfig()
      override;
  const mojo::Remote<chromeos::machine_learning::mojom::GraphExecutor>&
  GetExecutor() override;

 private:
  // Loads the built-in preprocessor config if not loaded yet. Also
  // initializes the model_ and executor_ with built-in model.
  void LazyInitialize();

  DISALLOW_COPY_AND_ASSIGN(BuiltinWorker);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_BUILTIN_WORKER_H_
