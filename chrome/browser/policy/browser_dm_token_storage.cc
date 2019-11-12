// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

namespace policy {

namespace {

constexpr char kInvalidTokenValue[] = "INVALID_DM_TOKEN";

void OnHardwarePlatformInfo(base::OnceClosure quit_closure,
                            std::string* out,
                            base::SysInfo::HardwareInfo info) {
  *out = info.serial_number;
  std::move(quit_closure).Run();
}
}  // namespace

using BrowserDMToken = BrowserDMTokenStorage::BrowserDMToken;

// static
BrowserDMTokenStorage* BrowserDMTokenStorage::storage_for_testing_ = nullptr;

BrowserDMToken BrowserDMToken::CreateValidToken(const std::string& dm_token) {
  DCHECK_NE(dm_token, kInvalidTokenValue);
  DCHECK(!dm_token.empty());
  return BrowserDMToken(Status::kValid, dm_token);
}

BrowserDMToken BrowserDMToken::CreateInvalidToken() {
  return BrowserDMToken(Status::kInvalid, "");
}

BrowserDMToken BrowserDMToken::CreateEmptyToken() {
  return BrowserDMToken(Status::kEmpty, "");
}

BrowserDMToken::BrowserDMToken(const BrowserDMToken& other) = default;

BrowserDMToken::BrowserDMToken(BrowserDMToken&& other) = default;

BrowserDMToken& BrowserDMToken::operator=(const BrowserDMToken& other) =
    default;

BrowserDMToken& BrowserDMToken::operator=(BrowserDMToken&& other) = default;

BrowserDMToken::~BrowserDMToken() = default;

const std::string& BrowserDMToken::value() const {
  // TODO(domfc): Uncomment DCHECK(is_valid()) after migrating code.
  // DCHECK(is_valid());
  return value_;
}

bool BrowserDMToken::is_valid() const {
  return status_ == Status::kValid;
}

bool BrowserDMToken::is_invalid() const {
  return status_ == Status::kInvalid;
}

bool BrowserDMToken::is_empty() const {
  return status_ == Status::kEmpty;
}

BrowserDMToken::BrowserDMToken(Status status, const base::StringPiece value)
    : status_(status), value_(value) {}

BrowserDMTokenStorage::BrowserDMTokenStorage()
    : is_initialized_(false), dm_token_(BrowserDMToken::CreateEmptyToken()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);

  // We don't call InitIfNeeded() here so that the global instance can be
  // created early during startup if needed. The tokens and client ID are read
  // from the system as part of the first retrieve or store operation.
}

BrowserDMTokenStorage::~BrowserDMTokenStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::string BrowserDMTokenStorage::RetrieveClientId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InitIfNeeded();
  return client_id_;
}

std::string BrowserDMTokenStorage::RetrieveSerialNumber() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!serial_number_) {
    serial_number_ = InitSerialNumber();
    DVLOG(1) << "Serial number= " << serial_number_.value();
  }

  return serial_number_.value();
}

std::string BrowserDMTokenStorage::RetrieveEnrollmentToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InitIfNeeded();
  return enrollment_token_;
}

void BrowserDMTokenStorage::StoreDMToken(const std::string& dm_token,
                                         StoreCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!store_callback_);
  InitIfNeeded();

  store_callback_ = std::move(callback);

  if (dm_token.empty()) {
    dm_token_ = BrowserDMToken::CreateEmptyToken();
    SaveDMToken("");
  } else if (dm_token == kInvalidTokenValue) {
    dm_token_ = BrowserDMToken::CreateInvalidToken();
    SaveDMToken(kInvalidTokenValue);
  } else {
    dm_token_ = BrowserDMToken::CreateValidToken(dm_token);
    SaveDMToken(dm_token_.value());
  }
}

std::string BrowserDMTokenStorage::RetrieveDMToken() {
  return RetrieveBrowserDMToken().value();
}

BrowserDMToken BrowserDMTokenStorage::RetrieveBrowserDMToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!store_callback_);

  InitIfNeeded();
  return dm_token_;
}

void BrowserDMTokenStorage::OnDMTokenStored(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(store_callback_);

  if (!store_callback_.is_null())
    std::move(store_callback_).Run(success);
}

bool BrowserDMTokenStorage::ShouldDisplayErrorMessageOnFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  InitIfNeeded();
  return should_display_error_message_on_failure_;
}

void BrowserDMTokenStorage::InitIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_initialized_)
    return;

  is_initialized_ = true;

  // Only supported in official builds.
  client_id_ = InitClientId();
  DVLOG(1) << "Client ID = " << client_id_;
  if (client_id_.empty())
    return;

  enrollment_token_ = InitEnrollmentToken();
  DVLOG(1) << "Enrollment token = " << enrollment_token_;

  std::string init_dm_token = InitDMToken();
  if (init_dm_token.empty())
    dm_token_ = BrowserDMToken::CreateEmptyToken();
  else if (init_dm_token == kInvalidTokenValue)
    dm_token_ = BrowserDMToken::CreateInvalidToken();
  else
    dm_token_ = BrowserDMToken::CreateValidToken(init_dm_token);

  if (dm_token_.is_valid())
    DVLOG(1) << "DM Token = " << dm_token_.value();
  else if (dm_token_.is_empty())
    DVLOG(1) << "DM Token = empty";
  else if (dm_token_.is_invalid())
    DVLOG(1) << "DM Token = invalid";
  else
    DVLOG(1) << "DM Token = unknown status";

  should_display_error_message_on_failure_ = InitEnrollmentErrorOption();
}

std::string BrowserDMTokenStorage::InitSerialNumber() {
  // GetHardwareInfo is asynchronous, but we need this synchronously. This call
  // will only happens once, as we cache the value. This will eventually be
  // moved earlier in Chrome's startup as it will be needed by the registration
  // as well.
  // TODO(crbug.com/907518): Move this earlier and make it async.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  std::string serial_number;
  base::SysInfo::GetHardwareInfo(base::BindOnce(
      &OnHardwarePlatformInfo, run_loop.QuitClosure(), &serial_number));

  run_loop.Run();

  return serial_number;
}

}  // namespace policy
