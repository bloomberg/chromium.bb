// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/heartbeat_scheduler.h"

#include <string>

#include "base/command_line.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/common/chrome_switches.h"
#include "components/gcm_driver/gcm_driver.h"

namespace {
const int kMinHeartbeatIntervalMs = 30 * 1000;  // 30 seconds
const int kMaxHeartbeatIntervalMs = 24 * 60 * 60 * 1000;  // 24 hours

// Our sender ID we send up with all of our GCM messages.
const char* kHeartbeatGCMAppID = "com.google.chromeos.monitoring";

// The default destination we send our GCM messages to.
const char* kHeartbeatGCMDestinationID = "1013309121859";
const char* kHeartbeatGCMSenderSuffix = "@google.com";

const char* kMonitoringMessageTypeKey = "type";
const char* kHeartbeatTimestampKey = "timestamp";
const char* kHeartbeatDomainNameKey = "domain_name";
const char* kHeartbeatDeviceIDKey = "device_id";
const char* kHeartbeatTypeValue = "hb";

// If we get an error registering with GCM, try again in two minutes.
const int64 kRegistrationRetryDelayMs = 2 * 60 * 1000;

// Returns the destination ID for GCM heartbeats.
std::string GetDestinationID() {
  std::string receiver_id = kHeartbeatGCMDestinationID;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMonitoringDestinationID)) {
    receiver_id = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kMonitoringDestinationID);
  }
  return receiver_id;
}

}  // namespace

namespace policy {

const int64 HeartbeatScheduler::kDefaultHeartbeatIntervalMs =
    2 * 60 * 1000; // 2 minutes

// Helper class used to manage GCM registration (handles retrying after
// errors, etc).
class HeartbeatRegistrationHelper {
 public:
  typedef base::Callback<void(const std::string& registration_id)>
      RegistrationHelperCallback;

  HeartbeatRegistrationHelper(
      gcm::GCMDriver* gcm_driver,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  void Register(const RegistrationHelperCallback& callback);

 private:
  void AttemptRegistration();

  // Callback invoked once a registration attempt has finished.
  void OnRegisterAttemptComplete(const std::string& registration_id,
                                 gcm::GCMClient::Result result);

  // GCMDriver to use to register.
  gcm::GCMDriver* const gcm_driver_;

  // Callback to invoke when we have completed GCM registration.
  RegistrationHelperCallback callback_;

  // TaskRunner used for scheduling retry attempts.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Should remain the last member so it will be destroyed first and
  // invalidate all weak pointers.
  base::WeakPtrFactory<HeartbeatRegistrationHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HeartbeatRegistrationHelper);
};

HeartbeatRegistrationHelper::HeartbeatRegistrationHelper(
    gcm::GCMDriver* gcm_driver,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : gcm_driver_(gcm_driver),
      task_runner_(task_runner),
      weak_factory_(this) {
}

void HeartbeatRegistrationHelper::Register(
    const RegistrationHelperCallback& callback) {
  // Should only call Register() once.
  DCHECK(callback_.is_null());
  callback_ = callback;
  AttemptRegistration();
}

void HeartbeatRegistrationHelper::AttemptRegistration() {
  std::vector<std::string> destinations;
  destinations.push_back(GetDestinationID());
  gcm_driver_->Register(
      kHeartbeatGCMAppID,
      destinations,
      base::Bind(&HeartbeatRegistrationHelper::OnRegisterAttemptComplete,
                 weak_factory_.GetWeakPtr()));
}

void HeartbeatRegistrationHelper::OnRegisterAttemptComplete(
    const std::string& registration_id, gcm::GCMClient::Result result) {
  DVLOG(1) << "Received Register() response: " << result;
  // TODO(atwilson): Track GCM errors via UMA (http://crbug.com/459238).
  switch (result) {
    case gcm::GCMClient::SUCCESS:
      {
        // Copy the callback, because the callback may free this object and
        // we don't want to free the callback object and any bound variables
        // until the callback exits.
        RegistrationHelperCallback callback = callback_;
        callback.Run(registration_id);
        // This helper may be freed now, so do not access any member variables
        // after this point.
        return;
      }

    case gcm::GCMClient::NETWORK_ERROR:
    case gcm::GCMClient::SERVER_ERROR:
      // Transient error - try again after a delay.
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&HeartbeatRegistrationHelper::AttemptRegistration,
                     weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kRegistrationRetryDelayMs));
      break;

    case gcm::GCMClient::INVALID_PARAMETER:
    case gcm::GCMClient::UNKNOWN_ERROR:
    case gcm::GCMClient::GCM_DISABLED:
      // No need to bother retrying in the case of one of these fatal errors.
      // This means that heartbeats will remain disabled until the next
      // restart.
      DLOG(ERROR) << "Fatal GCM Registration error: " << result;
      break;

    case gcm::GCMClient::ASYNC_OPERATION_PENDING:
    case gcm::GCMClient::TTL_EXCEEDED:
    default:
      NOTREACHED() << "Unexpected GCMDriver::Register() result: " << result;
      break;
  }
}

HeartbeatScheduler::HeartbeatScheduler(
    gcm::GCMDriver* driver,
    const std::string& enrollment_domain,
    const std::string& device_id,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner),
      enrollment_domain_(enrollment_domain),
      device_id_(device_id),
      heartbeat_enabled_(false),
      heartbeat_interval_(base::TimeDelta::FromMilliseconds(
          kDefaultHeartbeatIntervalMs)),
      gcm_driver_(driver),
      weak_factory_(this) {
  // If no GCMDriver (e.g. this is loaded as part of an unrelated unit test)
  // do nothing as no heartbeats can be sent.
  if (!gcm_driver_)
    return;

  heartbeat_frequency_observer_ =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kHeartbeatFrequency,
          base::Bind(&HeartbeatScheduler::RefreshHeartbeatSettings,
                     base::Unretained(this)));

  heartbeat_enabled_observer_ =
      chromeos::CrosSettings::Get()->AddSettingsObserver(
          chromeos::kHeartbeatEnabled,
          base::Bind(&HeartbeatScheduler::RefreshHeartbeatSettings,
                     base::Unretained(this)));

  // Update the heartbeat frequency from settings. This will trigger a
  // heartbeat as appropriate once the settings have been refreshed.
  RefreshHeartbeatSettings();
}

void HeartbeatScheduler::RefreshHeartbeatSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  if (chromeos::CrosSettingsProvider::TRUSTED != settings->PrepareTrustedValues(
          base::Bind(&HeartbeatScheduler::RefreshHeartbeatSettings,
                     weak_factory_.GetWeakPtr()))) {
    return;
  }

  // CrosSettings are trusted - update our cached settings (we cache the
  // value because CrosSettings can become untrusted at arbitrary times and we
  // want to use the last trusted value).
  int frequency;
  if (settings->GetInteger(chromeos::kHeartbeatFrequency, &frequency))
    heartbeat_interval_ = EnsureValidHeartbeatInterval(
        base::TimeDelta::FromMilliseconds(frequency));

  bool enabled;
  if (settings->GetBoolean(chromeos::kHeartbeatEnabled, &enabled))
    heartbeat_enabled_ = enabled;

  if (!heartbeat_enabled_) {
    // Heartbeats are no longer enabled - cancel our callback and any
    // outstanding registration attempts and disconnect from GCM so the
    // connection can be shut down. If heartbeats are re-enabled later, we
    // will re-register with GCM.
    heartbeat_callback_.Cancel();
    ShutdownGCM();
  } else {
    // Schedule a new upload with the new frequency.
    ScheduleNextHeartbeat();
  }

  DVLOG(1) << "heartbeat enabled: " << heartbeat_enabled_;
  DVLOG(1) << "heartbeat frequency: " << heartbeat_interval_;
}

void HeartbeatScheduler::ShutdownGCM() {
  registration_helper_.reset();
  registration_id_.clear();
  if (registered_app_handler_) {
    registered_app_handler_ = false;
    gcm_driver_->RemoveAppHandler(kHeartbeatGCMAppID);
  }
}

base::TimeDelta HeartbeatScheduler::EnsureValidHeartbeatInterval(
    const base::TimeDelta& interval) {
  const base::TimeDelta min = base::TimeDelta::FromMilliseconds(
      kMinHeartbeatIntervalMs);
  const base::TimeDelta max = base::TimeDelta::FromMilliseconds(
      kMaxHeartbeatIntervalMs);
  if (interval < min) {
    DLOG(WARNING) << "Invalid heartbeat interval: " << interval;
    return min;
  }
  if (interval > max) {
    DLOG(WARNING) << "Invalid heartbeat interval: " << interval;
    return max;
  }
  return interval;
}

void HeartbeatScheduler::ScheduleNextHeartbeat() {
  // Do nothing if heartbeats are disabled.
  if (!heartbeat_enabled_)
    return;

  if (registration_id_.empty()) {
    // We are not registered with the GCM service yet, so kick off registration.
    if (!registration_helper_) {
      // Add ourselves as an AppHandler - this is required in order to setup
      // a GCM connection.
      registered_app_handler_ = true;
      gcm_driver_->AddAppHandler(kHeartbeatGCMAppID, this);
      registration_helper_.reset(new HeartbeatRegistrationHelper(
          gcm_driver_, task_runner_));
      registration_helper_->Register(
          base::Bind(&HeartbeatScheduler::OnRegistrationComplete,
                     weak_factory_.GetWeakPtr()));
    }
    return;
  }

  // Calculate when to fire off the next update (if it should have already
  // happened, this yields a TimeDelta of 0).
  base::TimeDelta delay = std::max(
      last_heartbeat_ + heartbeat_interval_ - base::Time::NowFromSystemTime(),
      base::TimeDelta());

  heartbeat_callback_.Reset(base::Bind(&HeartbeatScheduler::SendHeartbeat,
                                       base::Unretained(this)));
  task_runner_->PostDelayedTask(
      FROM_HERE, heartbeat_callback_.callback(), delay);
}

void HeartbeatScheduler::OnRegistrationComplete(
    const std::string& registration_id) {
  DCHECK(!registration_id.empty());
  registration_helper_.reset();
  registration_id_ = registration_id;

  // Now that GCM registration is complete, start sending heartbeats.
  ScheduleNextHeartbeat();
}

void HeartbeatScheduler::SendHeartbeat() {
  DCHECK(!registration_id_.empty());
  if (!gcm_driver_ || !heartbeat_enabled_)
    return;

  gcm::GCMClient::OutgoingMessage message;
  message.time_to_live = heartbeat_interval_.InSeconds();
  // Just use the current timestamp as the message ID - if the user changes the
  // time and we send a message with the same ID that we previously used, no
  // big deal (the new message will replace the old, which is the behavior we
  // want anyway, per:
  // https://developer.chrome.com/apps/cloudMessaging#send_messages
  message.id = base::Int64ToString(
      base::Time::NowFromSystemTime().ToInternalValue());
  message.data[kMonitoringMessageTypeKey] = kHeartbeatTypeValue;
  message.data[kHeartbeatTimestampKey] = base::Int64ToString(
      base::Time::NowFromSystemTime().ToJavaTime());
  message.data[kHeartbeatDomainNameKey] = enrollment_domain_;
  message.data[kHeartbeatDeviceIDKey] = device_id_;
  gcm_driver_->Send(kHeartbeatGCMAppID,
                    GetDestinationID() + kHeartbeatGCMSenderSuffix,
                    message,
                    base::Bind(&HeartbeatScheduler::OnHeartbeatSent,
                               weak_factory_.GetWeakPtr()));
}

void HeartbeatScheduler::OnHeartbeatSent(const std::string& message_id,
                                         gcm::GCMClient::Result result) {
  DVLOG(1) << "Monitoring heartbeat sent - result = " << result;
  // Don't care if the result was successful or not - just schedule the next
  // heartbeat.
  DLOG_IF(ERROR, result != gcm::GCMClient::SUCCESS) <<
      "Error sending monitoring heartbeat: " << result;
  last_heartbeat_ = base::Time::NowFromSystemTime();
  ScheduleNextHeartbeat();
}

HeartbeatScheduler::~HeartbeatScheduler() {
  ShutdownGCM();
}

void HeartbeatScheduler::ShutdownHandler() {
  // This should never be called, because BrowserProcessImpl::StartTearDown()
  // should shutdown the BrowserPolicyConnector (which destroys this object)
  // before the GCMDriver. Our goal is to make sure that this object is always
  // shutdown before GCMDriver is shut down, rather than trying to handle the
  // case when GCMDriver goes away.
  NOTREACHED() << "HeartbeatScheduler should be destroyed before GCMDriver";
}

void HeartbeatScheduler::OnMessage(
    const std::string& app_id,
    const gcm::GCMClient::IncomingMessage& message) {
  // Should never be called because we don't get any incoming messages
  // for our app ID.
  NOTREACHED() << "Received incoming message for " << app_id;
}

void HeartbeatScheduler::OnMessagesDeleted(const std::string& app_id) {
}

void HeartbeatScheduler::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& details) {
  // Ignore send errors - we already are notified above in OnHeartbeatSent().
}

void HeartbeatScheduler::OnSendAcknowledged(const std::string& app_id,
                                            const std::string& message_id) {
  DVLOG(1) << "Heartbeat sent with message_id: " << message_id;
}

}  // namespace policy
