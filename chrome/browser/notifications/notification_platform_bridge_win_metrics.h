// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_METRICS_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_METRICS_H_

namespace notifications_uma {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DisplayStatus {
  SUCCESS = 0,
  RO_ACTIVATE_FAILED = 1,
  CONVERSION_FAILED_INSPECTABLE_TO_XML_IO = 2,
  LOAD_XML_FAILED = 3,
  CONVERSION_FAILED_XML_IO_TO_XML = 4,
  CREATE_FACTORY_FAILED = 5,
  CREATE_TOAST_NOTIFICATION_FAILED = 6,
  CREATE_TOAST_NOTIFICATION2_FAILED = 7,
  SETTING_GROUP_FAILED = 8,
  SETTING_TAG_FAILED = 9,
  GET_GROUP_FAILED = 10,
  GET_TAG_FAILED = 11,
  SUPPRESS_POPUP_FAILED = 12,
  ADD_TOAST_DISMISS_HANDLER_FAILED = 13,
  ADD_TOAST_ERROR_HANDLER_FAILED = 14,
  SHOWING_TOAST_FAILED = 15,
  CREATE_TOAST_NOTIFICATION_MANAGER_FAILED = 16,
  CREATE_TOAST_NOTIFIER_WITH_ID_FAILED = 17,
  COUNT  // Must be the final value.
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CloseStatus {
  SUCCESS = 0,
  GET_TOAST_HISTORY_FAILED = 1,
  REMOVING_TOAST_FAILED = 2,
  COUNT  // Must be the final value.
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HistoryStatus {
  SUCCESS = 0,
  CREATE_TOAST_NOTIFICATION_MANAGER_FAILED = 1,
  QUERY_TOAST_MANAGER_STATISTICS2_FAILED = 2,
  GET_TOAST_HISTORY_FAILED = 3,
  COUNT  // Must be the final value.
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetDisplayedStatus {
  SUCCESS = 0,
  SUCCESS_WITH_GET_AT_FAILURE = 1,
  GET_TOAST_HISTORY_FAILED = 2,
  QUERY_TOAST_NOTIFICATION_HISTORY2_FAILED = 3,
  GET_HISTORY_WITH_ID_FAILED = 4,
  GET_SIZE_FAILED = 5,
  COUNT  // Must be the final value.
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetDisplayedLaunchIdStatus {
  SUCCESS = 0,
  DECODE_LAUNCH_ID_FAILED = 1,
  COUNT  // Must be the final value.
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GetNotificationLaunchIdStatus {
  SUCCESS = 0,
  NOTIFICATION_GET_CONTENT_FAILED = 1,
  GET_ELEMENTS_BY_TAG_FAILED = 2,
  MISSING_TOAST_ELEMENT_IN_DOC = 3,
  ITEM_AT_FAILED = 4,
  GET_ATTRIBUTES_FAILED = 5,
  GET_NAMED_ITEM_FAILED = 6,
  GET_FIRST_CHILD_FAILED = 7,
  GET_NODE_VALUE_FAILED = 8,
  CONVERSION_TO_PROP_VALUE_FAILED = 9,
  GET_STRING_FAILED = 10,
  COUNT  // Must be the final value.
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ActivationStatus {
  SUCCESS = 0,
  GET_PROFILE_ID_INVALID_LAUNCH_ID = 1,
  ACTIVATION_INVALID_LAUNCH_ID = 2,
  COUNT  // Must be the final value.
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class HandleEventStatus {
  SUCCESS = 0,
  HANDLE_EVENT_LAUNCH_ID_INVALID = 1,
  COUNT  // Must be the final value.
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SetReadyCallbackStatus {
  SUCCESS = 0,
  COM_NOT_INITIALIZED = 1,
  COM_SERVER_MISCONFIGURATION = 2,
  SHORTCUT_MISCONFIGURATION = 3,
  COUNT  // Must be the final value.
};

// Methods to log histograms (to detect error rates in Native Notifications on
// Windows).
void LogDisplayHistogram(DisplayStatus status);
void LogCloseHistogram(CloseStatus status);
void LogHistoryHistogram(HistoryStatus status);
void LogGetDisplayedStatus(GetDisplayedStatus status);
void LogGetDisplayedLaunchIdStatus(GetDisplayedLaunchIdStatus status);
void LogGetNotificationLaunchIdStatus(GetNotificationLaunchIdStatus status);
void LogHandleEventStatus(HandleEventStatus status);
void LogActivationStatus(ActivationStatus status);
void LogSetReadyCallbackStatus(SetReadyCallbackStatus status);

}  // namespace notifications_uma

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_METRICS_H_
