// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_mac.h"

#include <string>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sha1.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/common/chrome_paths.h"

namespace policy {

namespace {

const char kDmTokenBaseDir[] =
    FILE_PATH_LITERAL("Google/Chrome Cloud Enrollment/");
const CFStringRef kEnrollmentTokenPolicyName =
    CFSTR("MachineLevelUserCloudPolicyEnrollmentToken");
const char kEnrollmentTokenFilePath[] =
#if defined(GOOGLE_CHROME_BUILD)
    FILE_PATH_LITERAL(
        "/Library/Google/Chrome/MachineLevelUserCloudPolicyEnrollmentToken");
#else
    FILE_PATH_LITERAL(
        "/Library/Application "
        "Support/Chromium/MachineLevelUserCloudPolicyEnrollmentToken");
#endif

bool GetDmTokenFilePath(base::FilePath* token_file_path,
                        const std::string& client_id,
                        bool create_dir) {
  if (!base::PathService::Get(base::DIR_APP_DATA, token_file_path))
    return false;

  *token_file_path = token_file_path->Append(kDmTokenBaseDir);

  if (create_dir && !base::CreateDirectory(*token_file_path))
    return false;

  std::string filename;
  base::Base64UrlEncode(base::SHA1HashString(client_id),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &filename);
  *token_file_path = token_file_path->Append(filename.c_str());

  return true;
}

bool StoreDMTokenInDirAppDataDir(const std::string& token,
                                 const std::string& client_id) {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, client_id, true)) {
    NOTREACHED();
    return false;
  }

  return base::ImportantFileWriter::WriteFileAtomically(token_file_path, token);
}

// Get the enrollment token from policy file: /Library/com.google.Chrome.plist.
// Return true if policy is set, otherwise false.
bool GetEnrollmentTokenFromPolicy(std::string* enrollment_token) {
// Since the configuration management infrastructure is not initialized when
// this code runs, read the policy preference directly.
#if defined(GOOGLE_CHROME_BUILD)
  // Explicitly access the "com.google.Chrome" bundle ID, no matter what this
  // app's bundle ID actually is. All channels of Chrome should obey the same
  // policies.
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
#else
  base::ScopedCFTypeRef<CFStringRef> bundle_id(
      base::SysUTF8ToCFStringRef(base::mac::BaseBundleID()));
#endif

  base::ScopedCFTypeRef<CFPropertyListRef> value(
      CFPreferencesCopyAppValue(kEnrollmentTokenPolicyName, bundle_id));

  if (!value ||
      !CFPreferencesAppValueIsForced(kEnrollmentTokenPolicyName, bundle_id)) {
    return false;
  }
  CFStringRef value_string = base::mac::CFCast<CFStringRef>(value);
  if (!value_string)
    return false;

  *enrollment_token = base::SysCFStringRefToUTF8(value_string);
  return true;
}

bool GetEnrollmentTokenFromFile(std::string* enrollment_token) {
  if (!base::ReadFileToString(base::FilePath(kEnrollmentTokenFilePath),
                              enrollment_token)) {
    return false;
  }
  *enrollment_token =
      base::TrimWhitespaceASCII(*enrollment_token, base::TRIM_ALL).as_string();
  return true;
}

}  // namespace

// static
BrowserDMTokenStorage* BrowserDMTokenStorage::Get() {
  if (storage_for_testing_)
    return storage_for_testing_;

  static base::NoDestructor<BrowserDMTokenStorageMac> storage;
  return storage.get();
}

BrowserDMTokenStorageMac::BrowserDMTokenStorageMac() : weak_factory_(this) {}

BrowserDMTokenStorageMac::~BrowserDMTokenStorageMac() {}

std::string BrowserDMTokenStorageMac::InitClientId() {
  // Returns the device s/n.
  base::mac::ScopedIOObject<io_service_t> expert_device(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPlatformExpertDevice")));
  if (!expert_device) {
    SYSLOG(ERROR) << "Error retrieving the machine serial number.";
    return std::string();
  }

  base::ScopedCFTypeRef<CFTypeRef> serial_number(
      IORegistryEntryCreateCFProperty(expert_device,
                                      CFSTR(kIOPlatformSerialNumberKey),
                                      kCFAllocatorDefault, 0));
  CFStringRef serial_number_cfstring =
      base::mac::CFCast<CFStringRef>(serial_number);
  if (!serial_number_cfstring) {
    SYSLOG(ERROR) << "Error retrieving the machine serial number.";
    return std::string();
  }

  ignore_result(serial_number.release());
  return base::SysCFStringRefToUTF8(serial_number_cfstring);
}

std::string BrowserDMTokenStorageMac::InitEnrollmentToken() {
  std::string enrollment_token;
  if (GetEnrollmentTokenFromPolicy(&enrollment_token))
    return enrollment_token;

  if (GetEnrollmentTokenFromFile(&enrollment_token))
    return enrollment_token;

  return std::string();
}

std::string BrowserDMTokenStorageMac::InitDMToken() {
  base::FilePath token_file_path;
  if (!GetDmTokenFilePath(&token_file_path, RetrieveClientId(), true))
    return std::string();

  std::string token;
  if (!base::ReadFileToString(token_file_path, &token))
    return std::string();

  return token;
}

void BrowserDMTokenStorageMac::SaveDMToken(const std::string& token) {
  std::string client_id = RetrieveClientId();
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&StoreDMTokenInDirAppDataDir, token, client_id),
      base::BindOnce(&BrowserDMTokenStorage::OnDMTokenStored,
                     weak_factory_.GetWeakPtr()));
}

}  // namespace policy
