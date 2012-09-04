// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_l10n.h"

#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace performance_monitor {

// Event-Related
string16 GetLocalizedStringFromEventCategory(const EventCategory category) {
  if (category == EVENT_CATEGORY_CHROME) {
    return l10n_util::GetStringFUTF16(
        IDS_PERFORMANCE_MONITOR_CHROME_EVENT_CATEGORY,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  }

  int string_id = 0;
  switch (category) {
    case EVENT_CATEGORY_EXTENSIONS:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSIONS_EVENT_CATEGORY;
      break;
    case EVENT_CATEGORY_EXCEPTIONS:
      string_id = IDS_PERFORMANCE_MONITOR_EXCEPTIONS_EVENT_CATEGORY;
      break;
    default:
      NOTREACHED();
  }

  return l10n_util::GetStringUTF16(string_id);
}

string16 GetLocalizedStringForEventCategoryDescription(
    const EventCategory category) {
  int string_id = 0;
  switch (category) {
    case EVENT_CATEGORY_EXTENSIONS:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSIONS_EVENT_CATEGORY_DESCRIPTION;
      break;
    case EVENT_CATEGORY_CHROME:
      string_id = IDS_PERFORMANCE_MONITOR_CHROME_EVENT_CATEGORY_DESCRIPTION;
      break;
    case EVENT_CATEGORY_EXCEPTIONS:
      string_id = IDS_PERFORMANCE_MONITOR_EXCEPTIONS_EVENT_CATEGORY_DESCRIPTION;
      break;
    default:
      NOTREACHED();
  }

  return l10n_util::GetStringFUTF16(
      string_id, l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
}

string16 GetLocalizedStringFromEventType(const EventType type) {
  int string_id = 0;

  switch (type) {
    case EVENT_EXTENSION_INSTALL:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_INSTALL_EVENT;
      break;
    case EVENT_EXTENSION_UNINSTALL:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_UNINSTALL_EVENT;
      break;
    case EVENT_EXTENSION_UPDATE:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_UPDATE_EVENT;
      break;
    case EVENT_EXTENSION_ENABLE:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_ENABLE_EVENT;
      break;
    case EVENT_EXTENSION_DISABLE:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_DISABLE_EVENT;
      break;
    case EVENT_CHROME_UPDATE:
      string_id = IDS_PERFORMANCE_MONITOR_CHROME_UPDATE_EVENT;
      break;
    case EVENT_RENDERER_FREEZE:
      string_id = IDS_PERFORMANCE_MONITOR_RENDERER_FREEZE_EVENT;
      break;
    case EVENT_RENDERER_CRASH:
      string_id = IDS_PERFORMANCE_MONITOR_RENDERER_CRASH_EVENT;
      break;
    case EVENT_KILLED_BY_OS_CRASH:
      string_id = IDS_PERFORMANCE_MONITOR_KILLED_BY_OS_CRASH_EVENT;
      break;
    case EVENT_UNCLEAN_EXIT:
      string_id = IDS_PERFORMANCE_MONITOR_UNCLEAN_EXIT_EVENT;
      break;
    default:
      NOTREACHED();
  }

  return l10n_util::GetStringUTF16(string_id);
}

string16 GetLocalizedStringForEventTypeDescription(const EventType type) {
  int string_id1 = 0;
  int string_id2 = 0;

  switch (type) {
    case EVENT_EXTENSION_INSTALL:
      string_id1 = IDS_PERFORMANCE_MONITOR_EXTENSION_INSTALL_EVENT_DESCRIPTION;
      break;
    case EVENT_EXTENSION_UNINSTALL:
      string_id1 =
          IDS_PERFORMANCE_MONITOR_EXTENSION_UNINSTALL_EVENT_DESCRIPTION;
      break;
    case EVENT_EXTENSION_UPDATE:
      string_id1 = IDS_PERFORMANCE_MONITOR_EXTENSION_UPDATE_EVENT_DESCRIPTION;
      break;
    case EVENT_EXTENSION_ENABLE:
      string_id1 = IDS_PERFORMANCE_MONITOR_EXTENSION_ENABLE_EVENT_DESCRIPTION;
      break;
    case EVENT_EXTENSION_DISABLE:
      string_id1 = IDS_PERFORMANCE_MONITOR_EXTENSION_DISABLE_EVENT_DESCRIPTION;
      break;
    case EVENT_CHROME_UPDATE:
      string_id1 = IDS_PERFORMANCE_MONITOR_CHROME_UPDATE_EVENT_DESCRIPTION;
      string_id2 = IDS_SHORT_PRODUCT_NAME;
      break;
    case EVENT_RENDERER_FREEZE:
      string_id1 = IDS_PERFORMANCE_MONITOR_RENDERER_FREEZE_EVENT_DESCRIPTION;
      break;
    case EVENT_RENDERER_CRASH:
      string_id1 = IDS_PERFORMANCE_MONITOR_RENDERER_CRASH_EVENT_DESCRIPTION;
      string_id2 = IDS_SAD_TAB_TITLE;
      break;
    case EVENT_KILLED_BY_OS_CRASH:
      string_id1 = IDS_PERFORMANCE_MONITOR_KILLED_BY_OS_CRASH_EVENT_DESCRIPTION;
      string_id2 = IDS_KILLED_TAB_TITLE;
      break;
    case EVENT_UNCLEAN_EXIT:
      string_id1 = IDS_PERFORMANCE_MONITOR_UNCLEAN_EXIT_EVENT_DESCRIPTION;
      string_id2 = IDS_SHORT_PRODUCT_NAME;
      break;
    default:
      NOTREACHED();
  }

  return string_id2 ?
         l10n_util::GetStringFUTF16(
             string_id1, l10n_util::GetStringUTF16(string_id2)) :
         l10n_util::GetStringUTF16(string_id1);
}

string16 GetLocalizedStringForEventTypeMouseover(const EventType type) {
  if (type == EVENT_CHROME_UPDATE) {
    return l10n_util::GetStringFUTF16(
        IDS_PERFORMANCE_MONITOR_CHROME_UPDATE_EVENT_MOUSEOVER,
        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
  }

  int string_id = 0;
  switch (type) {
    case EVENT_EXTENSION_INSTALL:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_INSTALL_EVENT_MOUSEOVER;
      break;
    case EVENT_EXTENSION_UNINSTALL:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_UNINSTALL_EVENT_MOUSEOVER;
      break;
    case EVENT_EXTENSION_UPDATE:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_UPDATE_EVENT_MOUSEOVER;
      break;
    case EVENT_EXTENSION_ENABLE:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_ENABLE_EVENT_MOUSEOVER;
      break;
    case EVENT_EXTENSION_DISABLE:
      string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_DISABLE_EVENT_MOUSEOVER;
      break;
    case EVENT_RENDERER_FREEZE:
      string_id = IDS_PERFORMANCE_MONITOR_RENDERER_FREEZE_EVENT_MOUSEOVER;
      break;
    case EVENT_RENDERER_CRASH:
      string_id = IDS_PERFORMANCE_MONITOR_RENDERER_CRASH_EVENT_MOUSEOVER;
      break;
    case EVENT_KILLED_BY_OS_CRASH:
      string_id = IDS_PERFORMANCE_MONITOR_KILLED_BY_OS_CRASH_EVENT_MOUSEOVER;
      break;
    case EVENT_UNCLEAN_EXIT:
      string_id = IDS_PERFORMANCE_MONITOR_UNCLEAN_EXIT_EVENT_MOUSEOVER;
      break;
    default:
      NOTREACHED();
  }

  return l10n_util::GetStringUTF16(string_id);
}

string16 GetLocalizedStringFromEventProperty(const std::string& property) {
  int string_id = 0;

  if (property == "extensionId")
    string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_ID_MOUSEOVER;
  else if (property == "extensionName")
    string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_NAME_MOUSEOVER;
  else if (property == "extensionUrl")
    string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_URL_MOUSEOVER;
  else if (property == "extensionLocation")
    string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_LOCATION_MOUSEOVER;
  else if (property == "extensionVersion")
    string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_VERSION_MOUSEOVER;
  else if (property == "extensionDescription")
    string_id = IDS_PERFORMANCE_MONITOR_EXTENSION_DESCRIPTION_MOUSEOVER;
  else if (property == "previousVersion")
    string_id = IDS_PERFORMANCE_MONITOR_PREVIOUS_VERSION_MOUSEOVER;
  else if (property == "currentVersion")
    string_id = IDS_PERFORMANCE_MONITOR_CURRENT_VERSION_MOUSEOVER;
  else if (property == "url")
    string_id = IDS_PERFORMANCE_MONITOR_URL_MOUSEOVER;
  else if (property == "profileName")
    string_id = IDS_PERFORMANCE_MONITOR_PROFILE_NAME_MOUSEOVER;
  else
    NOTREACHED();

  return l10n_util::GetStringUTF16(string_id);
}

// Metric-Related
string16 GetLocalizedStringFromMetricCategory(
    const MetricCategory category) {
  int string_id = 0;

  switch (category) {
    case METRIC_CATEGORY_CPU:
      string_id = IDS_PERFORMANCE_MONITOR_CPU_METRIC_CATEGORY;
      break;
    case METRIC_CATEGORY_MEMORY:
      string_id = IDS_PERFORMANCE_MONITOR_MEMORY_METRIC_CATEGORY;
      break;
    case METRIC_CATEGORY_TIMING:
      string_id = IDS_PERFORMANCE_MONITOR_TIMING_METRIC_CATEGORY;
      break;
    case METRIC_CATEGORY_NETWORK:
      string_id = IDS_PERFORMANCE_MONITOR_NETWORK_METRIC_CATEGORY;
      break;
    default:
      NOTREACHED();
  }

  return l10n_util::GetStringUTF16(string_id);
}

string16 GetLocalizedStringForMetricCategoryDescription(
    const MetricCategory category) {
  int string_id = 0;

  switch (category) {
    case METRIC_CATEGORY_CPU:
      string_id = IDS_PERFORMANCE_MONITOR_CPU_METRIC_CATEGORY_DESCRIPTION;
      break;
    case METRIC_CATEGORY_MEMORY:
      string_id = IDS_PERFORMANCE_MONITOR_MEMORY_METRIC_CATEGORY_DESCRIPTION;
      break;
    case METRIC_CATEGORY_TIMING:
      string_id = IDS_PERFORMANCE_MONITOR_TIMING_METRIC_CATEGORY_DESCRIPTION;
      break;
    case METRIC_CATEGORY_NETWORK:
      string_id = IDS_PERFORMANCE_MONITOR_NETWORK_METRIC_CATEGORY_DESCRIPTION;
      break;
    default:
      NOTREACHED();
  }

  return l10n_util::GetStringFUTF16(
      string_id, l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
}

string16 GetLocalizedStringFromMetricType(const MetricType type) {
  int string_id = 0;

  switch (type) {
    case METRIC_CPU_USAGE:
      string_id = IDS_PERFORMANCE_MONITOR_CPU_USAGE_METRIC;
      break;
    case METRIC_PRIVATE_MEMORY_USAGE:
      string_id = IDS_PERFORMANCE_MONITOR_PRIVATE_MEMORY_USAGE_METRIC;
      break;
    case METRIC_SHARED_MEMORY_USAGE:
      string_id = IDS_PERFORMANCE_MONITOR_SHARED_MEMORY_USAGE_METRIC;
      break;
    case METRIC_STARTUP_TIME:
      string_id = IDS_PERFORMANCE_MONITOR_STARTUP_TIME_METRIC;
      break;
    case METRIC_TEST_STARTUP_TIME:
      string_id = IDS_PERFORMANCE_MONITOR_TEST_STARTUP_TIME_METRIC;
      break;
    case METRIC_SESSION_RESTORE_TIME:
      string_id = IDS_PERFORMANCE_MONITOR_SESSION_RESTORE_TIME_METRIC;
      break;
    case METRIC_PAGE_LOAD_TIME:
      string_id = IDS_PERFORMANCE_MONITOR_PAGE_LOAD_TIME_METRIC;
      break;
    case METRIC_NETWORK_BYTES_READ:
      string_id = IDS_PERFORMANCE_MONITOR_NETWORK_BYTES_READ_METRIC;
      break;
    default:
      NOTREACHED();
  }

  return l10n_util::GetStringUTF16(string_id);
}

string16 GetLocalizedStringForMetricTypeDescription(const MetricType type) {
  int string_id = 0;

  switch (type) {
    case METRIC_CPU_USAGE:
      string_id = IDS_PERFORMANCE_MONITOR_CPU_USAGE_METRIC_DESCRIPTION;
      break;
    case METRIC_PRIVATE_MEMORY_USAGE:
      string_id =
          IDS_PERFORMANCE_MONITOR_PRIVATE_MEMORY_USAGE_METRIC_DESCRIPTION;
      break;
    case METRIC_SHARED_MEMORY_USAGE:
      string_id =
          IDS_PERFORMANCE_MONITOR_SHARED_MEMORY_USAGE_METRIC_DESCRIPTION;
      break;
    case METRIC_STARTUP_TIME:
      string_id = IDS_PERFORMANCE_MONITOR_STARTUP_TIME_METRIC_DESCRIPTION;
      break;
    case METRIC_TEST_STARTUP_TIME:
      string_id = IDS_PERFORMANCE_MONITOR_TEST_STARTUP_TIME_METRIC_DESCRIPTION;
      break;
    default:
      break;
  }

  if (string_id) {
    return l10n_util::GetStringFUTF16(
        string_id, l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
  }

  switch (type) {
    case METRIC_SESSION_RESTORE_TIME:
      string_id =
          IDS_PERFORMANCE_MONITOR_SESSION_RESTORE_TIME_METRIC_DESCRIPTION;
      break;
    case METRIC_PAGE_LOAD_TIME:
      string_id = IDS_PERFORMANCE_MONITOR_PAGE_LOAD_TIME_METRIC_DESCRIPTION;
      break;
    case METRIC_NETWORK_BYTES_READ:
      string_id = IDS_PERFORMANCE_MONITOR_NETWORK_BYTES_READ_METRIC_DESCRIPTION;
      break;
    default:
      NOTREACHED();
  }

  return l10n_util::GetStringUTF16(string_id);
}

// Miscellaneous
string16 GetLocalizedStringFromUnit(const Unit unit) {
  int string_id = 0;

  switch (unit) {
    case UNIT_BYTES:
      string_id = IDS_PERFORMANCE_MONITOR_BYTES_UNIT;
      break;
    case UNIT_KILOBYTES:
      string_id = IDS_PERFORMANCE_MONITOR_KILOBYTES_UNIT;
      break;
    case UNIT_MEGABYTES:
      string_id = IDS_PERFORMANCE_MONITOR_MEGABYTES_UNIT;
      break;
    case UNIT_GIGABYTES:
      string_id = IDS_PERFORMANCE_MONITOR_GIGABYTES_UNIT;
      break;
    case UNIT_TERABYTES:
      string_id = IDS_PERFORMANCE_MONITOR_TERABYTES_UNIT;
      break;
    case UNIT_MICROSECONDS:
      string_id = IDS_PERFORMANCE_MONITOR_MICROSECONDS_UNIT;
      break;
    case UNIT_MILLISECONDS:
      string_id = IDS_PERFORMANCE_MONITOR_MILLISECONDS_UNIT;
      break;
    case UNIT_SECONDS:
      string_id = IDS_PERFORMANCE_MONITOR_SECONDS_UNIT;
      break;
    case UNIT_MINUTES:
      string_id = IDS_PERFORMANCE_MONITOR_MINUTES_UNIT;
      break;
    case UNIT_HOURS:
      string_id = IDS_PERFORMANCE_MONITOR_HOURS_UNIT;
      break;
    case UNIT_DAYS:
      string_id = IDS_PERFORMANCE_MONITOR_DAYS_UNIT;
      break;
    case UNIT_WEEKS:
      string_id = IDS_PERFORMANCE_MONITOR_WEEKS_UNIT;
      break;
    case UNIT_MONTHS:
      string_id = IDS_PERFORMANCE_MONITOR_MONTHS_UNIT;
      break;
    case UNIT_YEARS:
      string_id = IDS_PERFORMANCE_MONITOR_YEARS_UNIT;
      break;
    case UNIT_PERCENT:
      string_id = IDS_PERFORMANCE_MONITOR_PERCENT_UNIT;
      break;
    default:
      NOTREACHED();
  }

  return l10n_util::GetStringUTF16(string_id);
}

}  // namespace performance_monitor
