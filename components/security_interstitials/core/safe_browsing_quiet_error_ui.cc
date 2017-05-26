// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/safe_browsing_quiet_error_ui.h"

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_interstitials {

SafeBrowsingQuietErrorUI::SafeBrowsingQuietErrorUI(
    const GURL& request_url,
    const GURL& main_frame_url,
    BaseSafeBrowsingErrorUI::SBInterstitialReason reason,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
    const std::string& app_locale,
    const base::Time& time_triggered,
    ControllerClient* controller,
    const bool is_giant_webview)
    : BaseSafeBrowsingErrorUI(request_url,
                              main_frame_url,
                              reason,
                              display_options,
                              app_locale,
                              time_triggered,
                              controller),
      is_giant_webview_(is_giant_webview) {
  controller->metrics_helper()->RecordUserDecision(MetricsHelper::SHOW);
  controller->metrics_helper()->RecordUserInteraction(
      MetricsHelper::TOTAL_VISITS);
  if (is_proceed_anyway_disabled()) {
    controller->metrics_helper()->RecordUserDecision(
        security_interstitials::MetricsHelper::PROCEEDING_DISABLED);
  }
}

SafeBrowsingQuietErrorUI::~SafeBrowsingQuietErrorUI() {
  controller()->metrics_helper()->RecordShutdownMetrics();
}

void SafeBrowsingQuietErrorUI::PopulateStringsForHtml(
    base::DictionaryValue* load_time_data) {
  DCHECK(load_time_data);

  load_time_data->SetString("type", "SAFEBROWSING");
  load_time_data->SetString(
      "tabTitle", l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_TITLE));
  load_time_data->SetBoolean("overridable", !is_proceed_anyway_disabled());
  load_time_data->SetString(
      "openDetails",
      l10n_util::GetStringUTF16(IDS_SAFEBROWSING_V3_OPEN_DETAILS_BUTTON));
  load_time_data->SetBoolean("is_giant", is_giant_webview_);

  bool phishing =
      interstitial_reason() == BaseSafeBrowsingErrorUI::SB_REASON_PHISHING;
  load_time_data->SetBoolean("phishing", phishing);
  load_time_data->SetString(
      "heading", phishing
                     ? l10n_util::GetStringUTF16(IDS_PHISHING_WEBVIEW_HEADING)
                     : l10n_util::GetStringUTF16(IDS_MALWARE_WEBVIEW_HEADING));

  int explanation_ids = -1;
  if (phishing)
    explanation_ids = IDS_PHISHING_WEBVIEW_EXPLANATION_PARAGRAPH;
  else if (interstitial_reason() == BaseSafeBrowsingErrorUI::SB_REASON_MALWARE)
    explanation_ids = IDS_MALWARE_WEBVIEW_EXPLANATION_PARAGRAPH;

  if (explanation_ids > -1) {
    load_time_data->SetString("explanationParagraph",
                              l10n_util::GetStringUTF16(explanation_ids));
  } else {
    NOTREACHED();
  }
}

void SafeBrowsingQuietErrorUI::SetGiantWebViewForTesting(
    bool is_giant_webview) {
  is_giant_webview_ = is_giant_webview;
}

void SafeBrowsingQuietErrorUI::HandleCommand(
    SecurityInterstitialCommands command) {
  NOTREACHED();
}

int SafeBrowsingQuietErrorUI::GetHTMLTemplateId() const {
  return IDR_SECURITY_INTERSTITIAL_QUIET_HTML;
};

}  // security_interstitials
