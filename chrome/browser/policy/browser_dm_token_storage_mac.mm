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
#import "base/mac/scoped_nsautorelease_pool.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/sha1.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/common/chrome_paths.h"

namespace policy {

namespace {

const char kDmTokenBaseDir[] =
    FILE_PATH_LITERAL("Google/Chrome Cloud Enrollment/");
NSString* const kEnrollmentTokenPolicyName =
    @"MachineLevelUserCloudPolicyEnrollmentToken";

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
  base::mac::ScopedNSAutoreleasePool pool;

  // Since the configuration management infrastructure is not initialized when
  // this code runs, read the policy preference directly.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* value = [defaults stringForKey:kEnrollmentTokenPolicyName];
  if (value && [defaults objectIsForcedForKey:kEnrollmentTokenPolicyName])
    return base::SysNSStringToUTF8(value);
  else
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
