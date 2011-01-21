// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/time_format.h"

#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "unicode/datefmt.h"
#include "unicode/locid.h"
#include "unicode/plurfmt.h"
#include "unicode/plurrule.h"
#include "unicode/smpdtfmt.h"

using base::Time;
using base::TimeDelta;

namespace {

static const char kFallbackFormatSuffixShort[] = "}";
static const char kFallbackFormatSuffixLeft[] = " left}";
static const char kFallbackFormatSuffixAgo[] = " ago}";

// Contains message IDs for various time units and pluralities.
struct MessageIDs {
  // There are 4 different time units and 6 different pluralities.
  int ids[4][6];
};

// Message IDs for different time formats.
static const MessageIDs kTimeShortMessageIDs = { {
  {
    IDS_TIME_SECS_DEFAULT, IDS_TIME_SEC_SINGULAR, IDS_TIME_SECS_ZERO,
    IDS_TIME_SECS_TWO, IDS_TIME_SECS_FEW, IDS_TIME_SECS_MANY
  },
  {
    IDS_TIME_MINS_DEFAULT, IDS_TIME_MIN_SINGULAR, IDS_TIME_MINS_ZERO,
    IDS_TIME_MINS_TWO, IDS_TIME_MINS_FEW, IDS_TIME_MINS_MANY
  },
  {
    IDS_TIME_HOURS_DEFAULT, IDS_TIME_HOUR_SINGULAR, IDS_TIME_HOURS_ZERO,
    IDS_TIME_HOURS_TWO, IDS_TIME_HOURS_FEW, IDS_TIME_HOURS_MANY
  },
  {
    IDS_TIME_DAYS_DEFAULT, IDS_TIME_DAY_SINGULAR, IDS_TIME_DAYS_ZERO,
    IDS_TIME_DAYS_TWO, IDS_TIME_DAYS_FEW, IDS_TIME_DAYS_MANY
  }
} };

static const MessageIDs kTimeRemainingMessageIDs = { {
  {
    IDS_TIME_REMAINING_SECS_DEFAULT, IDS_TIME_REMAINING_SEC_SINGULAR,
    IDS_TIME_REMAINING_SECS_ZERO, IDS_TIME_REMAINING_SECS_TWO,
    IDS_TIME_REMAINING_SECS_FEW, IDS_TIME_REMAINING_SECS_MANY
  },
  {
    IDS_TIME_REMAINING_MINS_DEFAULT, IDS_TIME_REMAINING_MIN_SINGULAR,
    IDS_TIME_REMAINING_MINS_ZERO, IDS_TIME_REMAINING_MINS_TWO,
    IDS_TIME_REMAINING_MINS_FEW, IDS_TIME_REMAINING_MINS_MANY
  },
  {
    IDS_TIME_REMAINING_HOURS_DEFAULT, IDS_TIME_REMAINING_HOUR_SINGULAR,
    IDS_TIME_REMAINING_HOURS_ZERO, IDS_TIME_REMAINING_HOURS_TWO,
    IDS_TIME_REMAINING_HOURS_FEW, IDS_TIME_REMAINING_HOURS_MANY
  },
  {
    IDS_TIME_REMAINING_DAYS_DEFAULT, IDS_TIME_REMAINING_DAY_SINGULAR,
    IDS_TIME_REMAINING_DAYS_ZERO, IDS_TIME_REMAINING_DAYS_TWO,
    IDS_TIME_REMAINING_DAYS_FEW, IDS_TIME_REMAINING_DAYS_MANY
  }
} };

static const MessageIDs kTimeElapsedMessageIDs = { {
  {
    IDS_TIME_ELAPSED_SECS_DEFAULT, IDS_TIME_ELAPSED_SEC_SINGULAR,
    IDS_TIME_ELAPSED_SECS_ZERO, IDS_TIME_ELAPSED_SECS_TWO,
    IDS_TIME_ELAPSED_SECS_FEW, IDS_TIME_ELAPSED_SECS_MANY
  },
  {
    IDS_TIME_ELAPSED_MINS_DEFAULT, IDS_TIME_ELAPSED_MIN_SINGULAR,
    IDS_TIME_ELAPSED_MINS_ZERO, IDS_TIME_ELAPSED_MINS_TWO,
    IDS_TIME_ELAPSED_MINS_FEW, IDS_TIME_ELAPSED_MINS_MANY
  },
  {
    IDS_TIME_ELAPSED_HOURS_DEFAULT, IDS_TIME_ELAPSED_HOUR_SINGULAR,
    IDS_TIME_ELAPSED_HOURS_ZERO, IDS_TIME_ELAPSED_HOURS_TWO,
    IDS_TIME_ELAPSED_HOURS_FEW, IDS_TIME_ELAPSED_HOURS_MANY
  },
  {
    IDS_TIME_ELAPSED_DAYS_DEFAULT, IDS_TIME_ELAPSED_DAY_SINGULAR,
    IDS_TIME_ELAPSED_DAYS_ZERO, IDS_TIME_ELAPSED_DAYS_TWO,
    IDS_TIME_ELAPSED_DAYS_FEW, IDS_TIME_ELAPSED_DAYS_MANY
  }
} };

// Different format types.
enum FormatType {
  FORMAT_SHORT,
  FORMAT_REMAINING,
  FORMAT_ELAPSED,
};

}  // namespace

class TimeFormatter {
  public:
    const std::vector<icu::PluralFormat*>& formatter(FormatType format_type) {
      switch (format_type) {
        case FORMAT_SHORT:
          return short_formatter_;
        case FORMAT_REMAINING:
          return time_left_formatter_;
        case FORMAT_ELAPSED:
          return time_elapsed_formatter_;
        default:
          NOTREACHED();
          return short_formatter_;
      }
    }
  private:
    static const MessageIDs& GetMessageIDs(FormatType format_type) {
      switch (format_type) {
        case FORMAT_SHORT:
          return kTimeShortMessageIDs;
        case FORMAT_REMAINING:
          return kTimeRemainingMessageIDs;
        case FORMAT_ELAPSED:
          return kTimeElapsedMessageIDs;
        default:
          NOTREACHED();
          return kTimeShortMessageIDs;
      }
    }

    static const char* GetFallbackFormatSuffix(FormatType format_type) {
      switch (format_type) {
        case FORMAT_SHORT:
          return kFallbackFormatSuffixShort;
        case FORMAT_REMAINING:
          return kFallbackFormatSuffixLeft;
        case FORMAT_ELAPSED:
          return kFallbackFormatSuffixAgo;
        default:
          NOTREACHED();
          return kFallbackFormatSuffixShort;
      }
    }

    TimeFormatter() {
      BuildFormats(FORMAT_SHORT, &short_formatter_);
      BuildFormats(FORMAT_REMAINING, &time_left_formatter_);
      BuildFormats(FORMAT_ELAPSED, &time_elapsed_formatter_);
    }
    ~TimeFormatter() {
      STLDeleteContainerPointers(short_formatter_.begin(),
                                 short_formatter_.end());
      STLDeleteContainerPointers(time_left_formatter_.begin(),
                                 time_left_formatter_.end());
      STLDeleteContainerPointers(time_elapsed_formatter_.begin(),
                                 time_elapsed_formatter_.end());
    }
    friend struct base::DefaultLazyInstanceTraits<TimeFormatter>;

    std::vector<icu::PluralFormat*> short_formatter_;
    std::vector<icu::PluralFormat*> time_left_formatter_;
    std::vector<icu::PluralFormat*> time_elapsed_formatter_;
    static void BuildFormats(FormatType format_type,
                             std::vector<icu::PluralFormat*>* time_formats);
    static icu::PluralFormat* createFallbackFormat(
        const icu::PluralRules& rules, int index, FormatType format_type);

    DISALLOW_COPY_AND_ASSIGN(TimeFormatter);
};

static base::LazyInstance<TimeFormatter> g_time_formatter(
    base::LINKER_INITIALIZED);

void TimeFormatter::BuildFormats(
    FormatType format_type, std::vector<icu::PluralFormat*>* time_formats) {
  static const icu::UnicodeString kKeywords[] = {
    UNICODE_STRING_SIMPLE("other"), UNICODE_STRING_SIMPLE("one"),
    UNICODE_STRING_SIMPLE("zero"), UNICODE_STRING_SIMPLE("two"),
    UNICODE_STRING_SIMPLE("few"), UNICODE_STRING_SIMPLE("many")
  };
  UErrorCode err = U_ZERO_ERROR;
  scoped_ptr<icu::PluralRules> rules(
      icu::PluralRules::forLocale(icu::Locale::getDefault(), err));
  if (U_FAILURE(err)) {
    err = U_ZERO_ERROR;
    icu::UnicodeString fallback_rules("one: n is 1", -1, US_INV);
    rules.reset(icu::PluralRules::createRules(fallback_rules, err));
    DCHECK(U_SUCCESS(err));
  }

  const MessageIDs& message_ids = GetMessageIDs(format_type);

  for (int i = 0; i < 4; ++i) {
    icu::UnicodeString pattern;
    for (size_t j = 0; j < arraysize(kKeywords); ++j) {
      int msg_id = message_ids.ids[i][j];
      std::string sub_pattern = l10n_util::GetStringUTF8(msg_id);
      // NA means this keyword is not used in the current locale.
      // Even if a translator translated for this keyword, we do not
      // use it unless it's 'other' (j=0) or it's defined in the rules
      // for the current locale. Special-casing of 'other' will be removed
      // once ICU's isKeyword is fixed to return true for isKeyword('other').
      if (sub_pattern.compare("NA") != 0 &&
          (j == 0 || rules->isKeyword(kKeywords[j]))) {
          pattern += kKeywords[j];
          pattern += UNICODE_STRING_SIMPLE("{");
          pattern += icu::UnicodeString(sub_pattern.c_str(), "UTF-8");
          pattern += UNICODE_STRING_SIMPLE("}");
      }
    }
    icu::PluralFormat* format = new icu::PluralFormat(*rules, pattern, err);
    if (U_SUCCESS(err)) {
      time_formats->push_back(format);
    } else {
      delete format;
      time_formats->push_back(createFallbackFormat(*rules, i, format_type));
      // Reset it so that next ICU call can proceed.
      err = U_ZERO_ERROR;
    }
  }
}

// Create a hard-coded fallback plural format. This will never be called
// unless translators make a mistake.
icu::PluralFormat* TimeFormatter::createFallbackFormat(
    const icu::PluralRules& rules, int index, FormatType format_type) {
  static const icu::UnicodeString kUnits[4][2] = {
    { UNICODE_STRING_SIMPLE("sec"), UNICODE_STRING_SIMPLE("secs") },
    { UNICODE_STRING_SIMPLE("min"), UNICODE_STRING_SIMPLE("mins") },
    { UNICODE_STRING_SIMPLE("hour"), UNICODE_STRING_SIMPLE("hours") },
    { UNICODE_STRING_SIMPLE("day"), UNICODE_STRING_SIMPLE("days") }
  };
  icu::UnicodeString suffix(GetFallbackFormatSuffix(format_type), -1, US_INV);
  icu::UnicodeString pattern;
  if (rules.isKeyword(UNICODE_STRING_SIMPLE("one"))) {
    pattern += UNICODE_STRING_SIMPLE("one{# ") + kUnits[index][0] + suffix;
  }
  pattern += UNICODE_STRING_SIMPLE(" other{# ") + kUnits[index][1] + suffix;
  UErrorCode err = U_ZERO_ERROR;
  icu::PluralFormat* format = new icu::PluralFormat(rules, pattern, err);
  DCHECK(U_SUCCESS(err));
  return format;
}

static string16 FormatTimeImpl(const TimeDelta& delta, FormatType format_type) {
  if (delta.ToInternalValue() < 0) {
    NOTREACHED() << "Negative duration";
    return string16();
  }

  int number;

  const std::vector<icu::PluralFormat*>& formatters =
    g_time_formatter.Get().formatter(format_type);

  UErrorCode error = U_ZERO_ERROR;
  icu::UnicodeString time_string;
  // Less than a minute gets "X seconds left"
  if (delta.ToInternalValue() < Time::kMicrosecondsPerMinute) {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerSecond);
    time_string = formatters[0]->format(number, error);

  // Less than 1 hour gets "X minutes left".
  } else if (delta.ToInternalValue() < Time::kMicrosecondsPerHour) {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerMinute);
    time_string = formatters[1]->format(number, error);

  // Less than 1 day remaining gets "X hours left"
  } else if (delta.ToInternalValue() < Time::kMicrosecondsPerDay) {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerHour);
    time_string = formatters[2]->format(number, error);

  // Anything bigger gets "X days left"
  } else {
    number = static_cast<int>(delta.ToInternalValue() /
                              Time::kMicrosecondsPerDay);
    time_string = formatters[3]->format(number, error);
  }

  // With the fallback added, this should never fail.
  DCHECK(U_SUCCESS(error));
  int capacity = time_string.length() + 1;
  string16 result;
  time_string.extract(static_cast<UChar*>(
                      WriteInto(&result, capacity)),
                      capacity, error);
  DCHECK(U_SUCCESS(error));
  return result;
}

// static
string16 TimeFormat::TimeElapsed(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_ELAPSED);
}

// static
string16 TimeFormat::TimeRemaining(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_REMAINING);
}

// static
string16 TimeFormat::TimeRemainingShort(const TimeDelta& delta) {
  return FormatTimeImpl(delta, FORMAT_SHORT);
}

// static
string16 TimeFormat::RelativeDate(
    const Time& time,
    const Time* optional_midnight_today) {
  Time midnight_today = optional_midnight_today ? *optional_midnight_today :
      Time::Now().LocalMidnight();

  // Filter out "today" and "yesterday"
  if (time >= midnight_today)
    return l10n_util::GetStringUTF16(IDS_PAST_TIME_TODAY);
  else if (time >= midnight_today -
                   TimeDelta::FromMicroseconds(Time::kMicrosecondsPerDay))
    return l10n_util::GetStringUTF16(IDS_PAST_TIME_YESTERDAY);

  return string16();
}
