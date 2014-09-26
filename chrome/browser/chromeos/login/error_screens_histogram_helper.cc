// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"

#include "base/metrics/histogram.h"

namespace chromeos {

namespace {

static const char kOobeErrorScreensCounterPrefix[] = "OOBE.NetworkErrorShown.";
static const char kOobeTimeSpentOnErrorScreensPrefix[] =
    "OOBE.ErrorScreensTime.";

const base::TimeDelta time_min = base::TimeDelta::FromMilliseconds(10);
const base::TimeDelta time_max = base::TimeDelta::FromMinutes(3);
const int time_bucket_count = 50;

std::string ErrorToString(ErrorScreen::ErrorState error) {
  switch (error) {
    case ErrorScreen::ERROR_STATE_PORTAL:
      return ".Portal";
    case ErrorScreen::ERROR_STATE_OFFLINE:
      return ".Offline";
    case ErrorScreen::ERROR_STATE_PROXY:
      return ".Proxy";
    case ErrorScreen::ERROR_STATE_AUTH_EXT_TIMEOUT:
      return ".AuthExtTimeout";
    default:
      NOTREACHED() << "Invalid ErrorState " << error;
      return std::string();
  }
}

void StoreErrorScreenToHistogram(const std::string& screen_name,
                                 ErrorScreen::ErrorState error) {
  if (error <= ErrorScreen::ERROR_STATE_UNKNOWN ||
      error > ErrorScreen::ERROR_STATE_NONE)
    return;
  std::string histogram_name = kOobeErrorScreensCounterPrefix + screen_name;
  int boundary = ErrorScreen::ERROR_STATE_NONE + 1;
  // This comes from UMA_HISTOGRAM_ENUMERATION macros. Can't use it because of
  // non const histogram name.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name,
      1,
      boundary,
      boundary + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(error);
}

void StoreTimeOnErrorScreenToHistogram(const std::string& screen_name,
                                       ErrorScreen::ErrorState error,
                                       const base::TimeDelta& time_delta) {
  if (error <= ErrorScreen::ERROR_STATE_UNKNOWN ||
      error > ErrorScreen::ERROR_STATE_NONE)
    return;
  std::string histogram_name =
      kOobeTimeSpentOnErrorScreensPrefix + screen_name + ErrorToString(error);

  // This comes from UMA_HISTOGRAM_MEDIUM_TIMES macros. Can't use it because of
  // non const histogram name.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      histogram_name,
      time_min,
      time_max,
      time_bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);

  histogram->AddTime(time_delta);
}

}  // namespace

ErrorScreensHistogramHelper::ErrorScreensHistogramHelper(
    const std::string& screen_name)
    : screen_name_(screen_name),
      was_shown_(false),
      last_error_shown_(ErrorScreen::ERROR_STATE_NONE) {
}

void ErrorScreensHistogramHelper::OnScreenShow() {
  was_shown_ = true;
}

void ErrorScreensHistogramHelper::OnErrorShow(ErrorScreen::ErrorState error) {
  OnErrorShowTime(error, base::Time::Now());
}

void ErrorScreensHistogramHelper::OnErrorShowTime(ErrorScreen::ErrorState error,
                                                  base::Time now) {
  last_error_shown_ = error;
  if (error_screen_start_time.is_null())
    error_screen_start_time = now;
  StoreErrorScreenToHistogram(screen_name_, error);
}

void ErrorScreensHistogramHelper::OnErrorHide() {
  OnErrorHideTime(base::Time::Now());
}

void ErrorScreensHistogramHelper::OnErrorHideTime(base::Time now) {
  DCHECK(!error_screen_start_time.is_null());
  time_on_error_screens += now - error_screen_start_time;
  error_screen_start_time = base::Time();
}

ErrorScreensHistogramHelper::~ErrorScreensHistogramHelper() {
  if (was_shown_) {
    if (last_error_shown_ == ErrorScreen::ERROR_STATE_NONE) {
      StoreErrorScreenToHistogram(screen_name_, ErrorScreen::ERROR_STATE_NONE);
    } else {
      if (!error_screen_start_time.is_null()) {
        time_on_error_screens += base::Time::Now() - error_screen_start_time;
        error_screen_start_time = base::Time();
      }
      StoreTimeOnErrorScreenToHistogram(
          screen_name_, last_error_shown_, time_on_error_screens);
    }
  }
}

}  // namespace chromeos
