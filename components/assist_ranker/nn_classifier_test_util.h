// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_ASSIST_RANKER_NN_CLASSIFIER_TEST_UTIL_H_
#define COMPONENTS_ASSIST_RANKER_NN_CLASSIFIER_TEST_UTIL_H_

#include <vector>

#include "components/assist_ranker/proto/nn_classifier.pb.h"

namespace assist_ranker {
namespace nn_classifier {

// Creates a NNClassifierModel proto using a specified set of biases and
// weights.
NNClassifierModel CreateModel(
    const std::vector<float>& hidden_biases,
    const std::vector<std::vector<float>>& hidden_weights,
    const std::vector<float>& logits_biases,
    const std::vector<std::vector<float>>& logits_weights);

// Performs inference on the input vector using the specified model, and
// checks that the expected scores are output.
bool CheckInference(const NNClassifierModel& model,
                    const std::vector<float>& input,
                    const std::vector<float>& expected_scores);

}  // namespace nn_classifier
}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_NN_CLASSIFIER_TEST_UTIL_H_
