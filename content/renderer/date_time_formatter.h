// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DATE_TIME_FORMATTER_H_
#define CONTENT_RENDERER_DATE_TIME_FORMATTER_H_

#include <string>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "third_party/icu/public/common/unicode/unistr.h"
#include "third_party/icu/public/i18n/unicode/gregocal.h"
#include "ui/base/ime/text_input_type.h"

namespace WebKit {
struct WebDateTimeChooserParams;
}  // namespace WebKit

namespace content {

// Converts between a text string representing a date/time and
// a set of year/month/day/hour/minute/second and vice versa.
// It is timezone agnostic.
class CONTENT_EXPORT DateTimeFormatter {
 public:
  explicit DateTimeFormatter(const WebKit::WebDateTimeChooserParams& source);
  DateTimeFormatter(
      ui::TextInputType type,
      int year, int month, int day, int hour, int minute, int second);
  ~DateTimeFormatter();

  int GetYear() const;
  int GetMonth() const;
  int GetDay() const;
  int GetHour() const;
  int GetMinute() const;
  int GetSecond() const;
  ui::TextInputType GetType() const;
  const std::string& GetFormattedValue() const;

 private:
  void CreatePatternMap();
  bool ParseValues();
  const std::string FormatString() const;
  int ExtractValue(
      const icu::Calendar* calendar, UCalendarDateFields value) const;
  void ExtractType(const WebKit::WebDateTimeChooserParams& source);
  void ClearAll();

  ui::TextInputType type_;
  icu::UnicodeString patterns_[ui::TEXT_INPUT_TYPE_MAX + 1];
  int year_;
  int month_;
  int day_;
  int hour_;
  int minute_;
  int second_;
  const icu::UnicodeString* pattern_;
  std::string formatted_string_;

  DISALLOW_COPY_AND_ASSIGN(DateTimeFormatter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DATE_TIME_FORMATTER_H_
