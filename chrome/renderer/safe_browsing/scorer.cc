// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/scorer.h"

#include <math.h>

#include "base/logging.h"
#include "base/string_piece.h"
#include "chrome/renderer/safe_browsing/client_model.pb.h"
#include "chrome/renderer/safe_browsing/features.h"

namespace safe_browsing {

// Helper function which converts log odds to a probability in the range
// [0.0,1.0].
static double LogOdds2Prob(double log_odds) {
  // 709 = floor(1023*ln(2)).  2**1023 is the largest finite double.
  // Small log odds aren't a problem.  as the odds will be 0.  It's only
  // when we get +infinity for the odds, that odds/(odds+1) would be NaN.
  if (log_odds >= 709) {
    return 1.0;
  }
  double odds = exp(log_odds);
  return odds/(odds+1.0);
}

Scorer::Scorer(const base::StringPiece& model_str) {
  model_.reset(new ClientSideModel());
  if (!model_->ParseFromArray(model_str.data(), model_str.size()) ||
      !model_->IsInitialized()) {
    DLOG(ERROR) << "Unable to parse phishing model.  This Scorer object is "
                << "invalid.";
    model_.reset(NULL);
    return;
  }
  for (int i = 0; i < model_->page_term_size(); ++i) {
    page_terms_.insert(model_->hashes(model_->page_term(i)));
  }
  for (int i = 0; i < model_->page_word_size(); ++i) {
    page_words_.insert(model_->hashes(model_->page_word(i)));
  }
}

Scorer::~Scorer() {}

bool Scorer::HasValidModel() const {
  return (model_.get() != NULL);
}

double Scorer::ComputeScore(const FeatureMap& features) const {
  DCHECK(HasValidModel());
  if (!HasValidModel()) {
    return 0.0;  // Better safe than sorry.  We don't want to crash the browser.
  }
  double logodds = 0.0;
  for (int i = 0; i < model_->rule_size(); ++i) {
    logodds += ComputeRuleScore(model_->rule(i), features);
  }
  return LogOdds2Prob(logodds);
}

const base::hash_set<std::string>& Scorer::page_terms() const {
  DCHECK(HasValidModel());
  return page_terms_;
}

const base::hash_set<std::string>& Scorer::page_words() const {
  DCHECK(HasValidModel());
  return page_words_;
}

size_t Scorer::max_words_per_term() const {
  DCHECK(HasValidModel());
  return (HasValidModel() ? model_->max_words_per_term() : 0);
}

double Scorer::ComputeRuleScore(const ClientSideModel::Rule& rule,
                                const FeatureMap& features) const {
  DCHECK(HasValidModel());
  const base::hash_map<std::string, double>& feature_map = features.features();
  double rule_score = 1.0;
  for (int i = 0; i < rule.feature_size(); ++i) {
    base::hash_map<std::string, double>::const_iterator it = feature_map.find(
        model_->hashes(rule.feature(i)));
    if (it == feature_map.end() || it->second == 0.0) {
      // If the feature of the rule does not exist in the given feature map the
      // feature weight is considered to be zero.  If the feature weight is zero
      // we leave early since we know that the rule score will be zero.
      return 0.0;
    }
    rule_score *= it->second;
  }
  return rule_score * rule.weight();
}
}  // namespace safe_browsing
