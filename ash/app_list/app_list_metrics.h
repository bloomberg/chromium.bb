// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_METRICS_H_
#define ASH_APP_LIST_APP_LIST_METRICS_H_

#include "ash/app_list/app_list_export.h"

namespace app_list {

class AppListModel;
class SearchModel;
class SearchResult;

// The UMA histogram that logs the input latency from input event to the
// representation time of the shown launcher UI.
constexpr char kAppListShowInputLatencyHistogram[] =
    "Apps.AppListShow.InputLatency";

// The UMA histogram that logs the input latency from input event to the
// representation time of the dismissed launcher UI.
constexpr char kAppListHideInputLatencyHistogram[] =
    "Apps.AppListHide.InputLatency";

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppListZeroStateSearchResultUserActionType enum listing in
// tools/metrics/histograms/enums.xml.
enum class ZeroStateSearchResultUserActionType {
  kRemoveResult = 0,
  kAppendResult = 1,
  kMaxValue = kAppendResult,
};

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppListZeroStateResultRemovalConfirmation enum listing in
// tools/metrics/histograms/enums.xml.
enum class ZeroStateSearchResutRemovalConfirmation {
  kRemovalConfirmed = 0,
  kRemovalCanceled = 1,
  kMaxValue = kRemovalCanceled,
};

void RecordFolderShowHideAnimationSmoothness(int actual_frames,
                                             int ideal_duration_ms,
                                             float refresh_rate);

void RecordPaginationAnimationSmoothness(int actual_frames,
                                         int ideal_duration_ms,
                                         float refresh_rate);

void RecordZeroStateSearchResultUserActionHistogram(
    ZeroStateSearchResultUserActionType action);

void RecordZeroStateSearchResultRemovalHistogram(
    ZeroStateSearchResutRemovalConfirmation removal_decision);

APP_LIST_EXPORT void RecordSearchResultOpenSource(
    const SearchResult* result,
    const AppListModel* model,
    const SearchModel* search_model);

}  // namespace app_list

#endif  // ASH_APP_LIST_APP_LIST_METRICS_H_
