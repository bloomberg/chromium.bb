// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_METRICS_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_METRICS_H_

namespace TranslateManagerMetrics {

enum MetricsNameIndex {
  UMA_INITIATION_STATUS,
  UMA_MAX,
};

// When Chrome Translate is ready to translate a page, one of following reason
// decide the next browser action.
enum InitiationStatusType {
  INITIATION_STATUS_DISABLED_BY_PREFS,
  INITIATION_STATUS_DISABLED_BY_SWITCH,
  INITIATION_STATUS_DISABLED_BY_CONFIG,
  INITIATION_STATUS_LANGUAGE_IS_NOT_SUPPORTED,
  INITIATION_STATUS_URL_IS_NOT_SUPPORTED,
  INITIATION_STATUS_SIMILAR_LANGUAGES,
  INITIATION_STATUS_ACCEPT_LANGUAGES,

  INITIATION_STATUS_AUTO_BY_CONFIG,
  INITIATION_STATUS_AUTO_BY_LINK,
  INITIATION_STATUS_SHOW_INFOBAR,

  INITIATION_STATUS_MAX,
};

// Called when Chrome Translate is initiated to report a reason of the next
// browser action.
void ReportInitiationStatus(InitiationStatusType type);

// Provides UMA entry names for unit tests.
const char* GetMetricsName(MetricsNameIndex index);

}  // namespace TranslateManagerMetrics

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_METRICS_H_
