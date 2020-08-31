// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/search_result_parsers/kp_entity_result_parser.h"

#include <string>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace chromeos {
namespace quick_answers {
namespace {

using base::Value;

constexpr char kKnowledgeEntityPath[] = "knowledgePanelEntityResult.entity";
constexpr char kScorePath[] =
    "ratingsAndReviews.google.aggregateRating.averageScore";
constexpr char kRatingCountPath[] =
    "ratingsAndReviews.google.aggregateRating.aggregatedCount";
constexpr char kKnownForReasonPath[] = "localizedKnownForReason";
constexpr char kRatingReviewTemplate[] = "%.1f ★ (%s reviews)";

}  // namespace

// Extract |quick_answer| from knowledge panel entity result.
bool KpEntityResultParser::Parse(const Value* result,
                                 QuickAnswer* quick_answer) {
  const auto* entity = result->FindPath(kKnowledgeEntityPath);
  if (!entity) {
    LOG(ERROR) << "Can't find the knowledge panel entity.";
    return false;
  }

  const auto average_score = entity->FindDoublePath(kScorePath);
  const auto* aggregated_count = entity->FindStringPath(kRatingCountPath);

  if (average_score.has_value() && aggregated_count) {
    const auto& answer =
        base::StringPrintf(kRatingReviewTemplate, average_score.value(),
                           aggregated_count->c_str());
    quick_answer->primary_answer = answer;
    quick_answer->first_answer_row.push_back(
        std::make_unique<QuickAnswerResultText>(answer));
  } else {
    const std::string* localized_known_for_reason =
        entity->FindStringPath(kKnownForReasonPath);
    if (!localized_known_for_reason) {
      LOG(ERROR) << "Can't find the localized known for reason field.";
      return false;
    }

    quick_answer->primary_answer = *localized_known_for_reason;
    quick_answer->first_answer_row.push_back(
        std::make_unique<QuickAnswerResultText>(*localized_known_for_reason));
  }

  quick_answer->result_type = ResultType::kKnowledgePanelEntityResult;
  return true;
}

}  // namespace quick_answers
}  // namespace chromeos
