// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROXIMITY_AUTH_SMART_LOCK_METRICS_RECORDER_H_
#define CHROMEOS_COMPONENTS_PROXIMITY_AUTH_SMART_LOCK_METRICS_RECORDER_H_

class SmartLockMetricsRecorder {
 public:
  SmartLockMetricsRecorder();
  ~SmartLockMetricsRecorder();

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class SmartLockGetRemoteStatusResultFailureReason {
    kTimedOutBluetoothDisabled = 0,
    kTimedOutCouldNotEstablishAuthenticatedChannel = 1,
    kTimedOutDidNotReceiveRemoteStatusUpdate = 2,
    kUserEnteredPasswordWhileBluetoothDisabled = 3,
    kUserEnteredPasswordWhileConnecting = 4,
    kAuthenticatedChannelDropped = 5,
    kMaxValue = kAuthenticatedChannelDropped
  };

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other). Entries should be never modified
  // or deleted. Only additions possible.
  enum class SmartLockAuthResultFailureReason {
    kUnlockNotAllowed = 0,
    kAlreadyAttemptingAuth = 1,
    kEmptyUserAccount = 2,
    kInvalidAccoundId = 3,
    kAuthAttemptCannotStart = 4,
    kNoPendingOrActiveHost = 5,
    kAuthenticatedChannelDropped = 6,
    kFailedToSendUnlockRequest = 7,
    kFailedToDecryptSignInChallenge = 8,
    kFailedtoNotifyHostDeviceThatSmartLockWasUsed = 9,
    kAuthAttemptTimedOut = 10,
    kUnlockEventSentButNotAttemptingAuth = 11,
    kUnlockRequestSentButNotAttemptingAuth = 12,
    kLoginDisplayHostDoesNotExist = 13,
    kUserControllerSignInFailure = 14,
    kMaxValue = kUserControllerSignInFailure
  };

  static void RecordAuthResultUnlockSuccess(bool success = true);
  static void RecordAuthResultUnlockFailure(
      SmartLockAuthResultFailureReason failure_reason);

  static void RecordGetRemoteStatusResultUnlockSuccess(bool success = true);
  static void RecordGetRemoteStatusResultUnlockFailure(
      SmartLockGetRemoteStatusResultFailureReason failure_reason);

  static void RecordAuthResultSignInSuccess(bool success = true);
  static void RecordAuthResultSignInFailure(
      SmartLockAuthResultFailureReason failure_reason);

  static void RecordGetRemoteStatusResultSignInSuccess(bool success = true);
  static void RecordGetRemoteStatusResultSignInFailure(
      SmartLockGetRemoteStatusResultFailureReason failure_reason);
};

#endif  // CHROMEOS_COMPONENTS_PROXIMITY_AUTH_SMART_LOCK_METRICS_RECORDER_H_
