// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/assist_ranker/nn_classifier_test_util.h"

#include "components/assist_ranker/nn_classifier.h"

namespace assist_ranker {
namespace nn_classifier {
namespace {

using ::google::protobuf::RepeatedFieldBackInserter;
using ::std::copy;
using ::std::vector;

void CreateLayer(const vector<float>& biases,
                 const vector<vector<float>>& weights,
                 NNLayer* layer) {
  copy(biases.begin(), biases.end(),
       RepeatedFieldBackInserter(layer->mutable_biases()->mutable_values()));

  for (const auto& w : weights) {
    auto* p = layer->add_weights();
    copy(w.begin(), w.end(), RepeatedFieldBackInserter(p->mutable_values()));
  }
}

}  // namespace

NNClassifierModel CreateModel(const vector<float>& hidden_biases,
                              const vector<vector<float>>& hidden_weights,
                              const vector<float>& logits_biases,
                              const vector<vector<float>>& logits_weights) {
  NNClassifierModel model;
  CreateLayer(hidden_biases, hidden_weights, model.mutable_hidden_layer());
  CreateLayer(logits_biases, logits_weights, model.mutable_logits_layer());
  return model;
}

bool CheckInference(const NNClassifierModel& model,
                    const vector<float>& input,
                    const vector<float>& expected_scores) {
  const vector<float> scores = Inference(model, input);
  if (scores.size() != expected_scores.size())
    return false;
  for (size_t i = 0; i < scores.size(); ++i) {
    if (abs(scores[i] - expected_scores[i]) > 1e-05)
      return false;
  }

  return true;
}

}  // namespace nn_classifier
}  // namespace assist_ranker
