// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/scorer.h"

#include <math.h>

#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "chrome/renderer/safe_browsing/client_model.pb.h"
#include "chrome/renderer/safe_browsing/features.h"

namespace safe_browsing {

const int Scorer::kMaxPhishingModelSizeBytes = 70 * 1024;

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

// A helper class that manages a single asynchronous model load
// for Scorer::CreateFromFile().
namespace {
class ScorerLoader {
 public:
  // Creates a new ScorerLoader for loading the given file.
  // Parameters are the same as for Scorer::CreateFromFile().
  ScorerLoader(scoped_refptr<base::MessageLoopProxy> file_thread_proxy,
               base::PlatformFile model_file,
               Scorer::CreationCallback* creation_callback)
      : file_thread_proxy_(file_thread_proxy),
        model_file_(model_file),
        creation_callback_(creation_callback) {
    memset(buffer_, 0, sizeof(buffer_));
  }

  ~ScorerLoader() {}

  // Begins the model load.
  // The ScorerLoader will delete itself when the load is finished.
  void Run() {
    bool success = base::FileUtilProxy::Read(
        file_thread_proxy_,
        model_file_,
        0,  // offset
        buffer_,
        Scorer::kMaxPhishingModelSizeBytes,
        NewCallback(this, &ScorerLoader::ModelReadDone));
    DCHECK(success) << "Unable to post a task to read the phishing model file";
  }

 private:
  // Callback that is run once the file data has been read.
  void ModelReadDone(base::PlatformFileError error_code, int bytes_read) {
    Scorer* scorer = NULL;
    if (error_code != base::PLATFORM_FILE_OK) {
      DLOG(ERROR) << "Error reading phishing model file: " << error_code;
    } else if (bytes_read <= 0) {
      DLOG(ERROR) << "Empty phishing model file";
    } else if (bytes_read == Scorer::kMaxPhishingModelSizeBytes) {
      DLOG(ERROR) << "Phishing model is too large, ignoring";
    } else {
      scorer = Scorer::Create(base::StringPiece(buffer_, bytes_read));
    }
    RunCallback(scorer);
  }

  // Helper function to run the creation callback and delete the loader.
  void RunCallback(Scorer* scorer) {
    creation_callback_->Run(scorer);
    delete this;
  }

  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;
  base::PlatformFile model_file_;
  scoped_ptr<Scorer::CreationCallback> creation_callback_;
  char buffer_[Scorer::kMaxPhishingModelSizeBytes];

  DISALLOW_COPY_AND_ASSIGN(ScorerLoader);
};
}  // namespace


Scorer::Scorer() {}
Scorer::~Scorer() {}

/* static */
Scorer* Scorer::Create(const base::StringPiece& model_str) {
  scoped_ptr<Scorer> scorer(new Scorer());
  ClientSideModel& model = scorer->model_;
  if (!model.ParseFromArray(model_str.data(), model_str.size()) ||
      !model.IsInitialized()) {
    DLOG(ERROR) << "Unable to parse phishing model.  This Scorer object is "
                << "invalid.";
    return NULL;
  }
  for (int i = 0; i < model.page_term_size(); ++i) {
    scorer->page_terms_.insert(model.hashes(model.page_term(i)));
  }
  for (int i = 0; i < model.page_word_size(); ++i) {
    scorer->page_words_.insert(model.hashes(model.page_word(i)));
  }
  return scorer.release();
}

/* static */
void Scorer::CreateFromFile(
    base::PlatformFile model_file,
    scoped_refptr<base::MessageLoopProxy> file_thread_proxy,
    Scorer::CreationCallback* creation_callback) {
  ScorerLoader* loader = new ScorerLoader(file_thread_proxy,
                                          model_file,
                                          creation_callback);
  loader->Run();  // deletes itself when finished
}

double Scorer::ComputeScore(const FeatureMap& features) const {
  double logodds = 0.0;
  for (int i = 0; i < model_.rule_size(); ++i) {
    logodds += ComputeRuleScore(model_.rule(i), features);
  }
  return LogOdds2Prob(logodds);
}

const base::hash_set<std::string>& Scorer::page_terms() const {
  return page_terms_;
}

const base::hash_set<std::string>& Scorer::page_words() const {
  return page_words_;
}

size_t Scorer::max_words_per_term() const {
  return model_.max_words_per_term();
}

double Scorer::ComputeRuleScore(const ClientSideModel::Rule& rule,
                                const FeatureMap& features) const {
  const base::hash_map<std::string, double>& feature_map = features.features();
  double rule_score = 1.0;
  for (int i = 0; i < rule.feature_size(); ++i) {
    base::hash_map<std::string, double>::const_iterator it = feature_map.find(
        model_.hashes(rule.feature(i)));
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
