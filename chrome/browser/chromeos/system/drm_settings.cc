// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/drm_settings.h"

#include "base/bind.h"
#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/encryptor.h"
#include "crypto/sha2.h"

using content::BrowserThread;

namespace {

// This constant is mirrored in
//   content/browser/renderer_host/pepper_message_filter.cc
// for OnGetDeviceID.
//
// This ID file is solely for use via the private pepper API.
//
// NOTE! Changing this value will also change the generated value
//       do not do so without accounting for the change.
const char kDRMIdentifierFile[] = "Pepper DRM ID.0";

void ManageDrmIdentifierOnFileThread(bool enable, const std::string& email) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Drop the file under <data>/<profile>/<drm id file>.
  // TODO(wad) get the profile directory in a more succinct fashion.
  FilePath drm_id_file;
  PathService::Get(chrome::DIR_USER_DATA, &drm_id_file);
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  FilePath profile = cmd_line.GetSwitchValuePath(switches::kLoginProfile);
  if (profile.empty()) {
    LOG(ERROR) << "called with no login-profile!";
    return;
  }
  drm_id_file = drm_id_file.AppendASCII(profile.value());
  drm_id_file = drm_id_file.AppendASCII(kDRMIdentifierFile);

  // The file will be regenerated or deleted at toggle-time.
  file_util::Delete(drm_id_file, false);

  // If DRM support is disabled, then do nothing else.
  if (!enable)
    return;

  // Build the identifier as follows:
  // SHA256(system-salt||service||SHA256(system-salt||service||email))
  chromeos::CryptohomeLibrary* c_home =
    chromeos::CrosLibrary::Get()->GetCryptohomeLibrary();
  std::string salt = c_home->GetSystemSalt();
  char id_buf[256 / 8];  // 256-bits for SHA256
  std::string input = salt;
  input.append(kDRMIdentifierFile);
  input.append(email);
  crypto::SHA256HashString(input, &id_buf, sizeof(id_buf));
  std::string id = StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(id_buf),
        sizeof(id_buf)));
  input = salt;
  input.append(kDRMIdentifierFile);
  input.append(id);
  crypto::SHA256HashString(input, &id_buf, sizeof(id_buf));
  id = StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(id_buf),
        sizeof(id_buf)));

  if (file_util::WriteFile(drm_id_file, id.c_str(), id.length()) !=
      static_cast<int>(id.length())) {
    LOG(ERROR) << "Failed to write " << drm_id_file.value();
    return;
  }
}

}  // namespace

namespace chromeos {
namespace system {

void ToggleDrm(bool enable) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UserManager* user_manager = UserManager::Get();
  if (!user_manager)
    return;

  // This preference is only meant to be called when a user
  // is logged in.
  if (!user_manager->IsUserLoggedIn()) {
    LOG(WARNING) << "Called without a logged in user!";
    return;
  }

  // Don't generate files as a guest, demo user, or a stub.
  if (user_manager->IsLoggedInAsGuest() ||
      user_manager->IsLoggedInAsDemoUser() ||
      user_manager->IsLoggedInAsStub())
    return;

  // The user email address is included in the hash to keep the identifier
  // from being the same across users.
  std::string email = user_manager->GetLoggedInUser().email();
  // If there is no real user then there's nothing to do here.
  if (email.length() == 0) {
    LOG(WARNING) << "Unexpected 0 length user email.";
    return;
  }

  // Generate a DRM identifier on the FILE thread.
  // The DRM identifier is a per-user, per-OS-install identifier that is used
  // by privileged pepper plugins specifically for deriving
  // per-content-provider identifiers.  The user must be able to clear it,
  // reset it, and deny its use.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ManageDrmIdentifierOnFileThread, enable, email));
}

}  // namespace system
}  // namespace chromeos
