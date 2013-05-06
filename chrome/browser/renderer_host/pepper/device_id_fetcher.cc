// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/device_id_fetcher.h"

#include "base/file_util.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/pepper/pepper_flash_device_id_host.h"
#include "chrome/common/pref_names.h"
#if defined(OS_CHROMEOS)
#include "chromeos/cryptohome/cryptohome_library.h"
#endif
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "crypto/encryptor.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#if defined(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif

using content::BrowserPpapiHost;
using content::BrowserThread;
using content::RenderProcessHost;

namespace chrome {

namespace {

const char kDRMIdentifierFile[] = "Pepper DRM ID.0";

const uint32_t kSaltLength = 32;

}  // namespace

DeviceIDFetcher::DeviceIDFetcher(const IDCallback& callback,
                                 PP_Instance instance)
    : callback_(callback),
      instance_(instance) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

DeviceIDFetcher::~DeviceIDFetcher() {
}

void DeviceIDFetcher::Start(BrowserPpapiHost* browser_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  int process_id = 0;
  int unused = 0;
  browser_host->GetRenderViewIDsForInstance(instance_,
                                            &process_id,
                                            &unused);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DeviceIDFetcher::CheckPrefsOnUIThread, this, process_id));
}

// static
void DeviceIDFetcher::RegisterUserPrefs(
    user_prefs::PrefRegistrySyncable* prefs) {
  // TODO(wad): Once UI is connected, a final default can be set. At that point
  // change this pref from UNSYNCABLE to SYNCABLE.
  prefs->RegisterBooleanPref(prefs::kEnableDRM,
                             true,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(
      prefs::kDRMSalt,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
base::FilePath DeviceIDFetcher::GetLegacyDeviceIDPath(
    const base::FilePath& profile_path) {
  return profile_path.AppendASCII(kDRMIdentifierFile);
}

// TODO(raymes): Change this to just return the device id salt and call it with
// PostTaskAndReply once the legacy ChromeOS codepath is removed.
void DeviceIDFetcher::CheckPrefsOnUIThread(int process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = NULL;
  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(process_id);
  if (render_process_host && render_process_host->GetBrowserContext()) {
    profile = Profile::FromBrowserContext(
        render_process_host->GetBrowserContext());
  }

  if (!profile ||
      profile->IsOffTheRecord() ||
      !profile->GetPrefs()->GetBoolean(prefs::kEnableDRM)) {
    RunCallbackOnIOThread(std::string());
    return;
  }

  // Check if the salt pref is set. If it isn't, set it.
  std::string salt = profile->GetPrefs()->GetString(prefs::kDRMSalt);
  if (salt.empty()) {
    uint8_t salt_bytes[kSaltLength];
    crypto::RandBytes(salt_bytes, arraysize(salt_bytes));
    // Since it will be stored in a string pref, convert it to hex.
    salt = base::HexEncode(salt_bytes, arraysize(salt_bytes));
    profile->GetPrefs()->SetString(prefs::kDRMSalt, salt);
  }

#if defined(OS_CHROMEOS)
  // Try the legacy path first for ChromeOS. We pass the new salt in as well
  // in case the legacy id doesn't exist.
  BrowserThread::PostBlockingPoolTask(FROM_HERE,
      base::Bind(&DeviceIDFetcher::ComputeOnBlockingPool, this,
                 profile->GetPath(), salt));
#else
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DeviceIDFetcher::ComputeOnIOThread, this, salt));
#endif
}


void DeviceIDFetcher::ComputeOnIOThread(const std::string& salt) {
  std::vector<uint8> salt_bytes;
  if (!base::HexStringToBytes(salt, &salt_bytes))
    salt_bytes.clear();

  // Build the identifier as follows:
  // SHA256(machine-id||service||SHA256(machine-id||service||salt))
  std::string machine_id = GetMachineID();
  if (machine_id.empty() || salt_bytes.size() != kSaltLength) {
    NOTREACHED();
    RunCallbackOnIOThread(std::string());
    return;
  }

  char id_buf[256 / 8];  // 256-bits for SHA256
  std::string input = machine_id;
  input.append(kDRMIdentifierFile);
  input.append(salt_bytes.begin(), salt_bytes.end());
  crypto::SHA256HashString(input, &id_buf, sizeof(id_buf));
  std::string id = StringToLowerASCII(
      base::HexEncode(reinterpret_cast<const void*>(id_buf), sizeof(id_buf)));
  input = machine_id;
  input.append(kDRMIdentifierFile);
  input.append(id);
  crypto::SHA256HashString(input, &id_buf, sizeof(id_buf));
  id = StringToLowerASCII(base::HexEncode(
        reinterpret_cast<const void*>(id_buf),
        sizeof(id_buf)));

  RunCallbackOnIOThread(id);
}

// TODO(raymes): This is temporary code to migrate ChromeOS devices to the new
// scheme for generating device IDs. Delete this once we are sure most ChromeOS
// devices have been migrated.
void DeviceIDFetcher::ComputeOnBlockingPool(const base::FilePath& profile_path,
                                            const std::string& salt) {
  std::string id;
  // First check if the legacy device ID file exists on ChromeOS. If it does, we
  // should just return that.
  base::FilePath id_path = GetLegacyDeviceIDPath(profile_path);
  if (file_util::PathExists(id_path)) {
    if (file_util::ReadFileToString(id_path, &id) && !id.empty()) {
      RunCallbackOnIOThread(id);
      return;
    }
  }
  // If we didn't find an ID, go back to the new code path to generate an ID.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&DeviceIDFetcher::ComputeOnIOThread, this, salt));
}


void DeviceIDFetcher::RunCallbackOnIOThread(const std::string& id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DeviceIDFetcher::RunCallbackOnIOThread, this, id));
    return;
  }
  callback_.Run(id);
}

std::string DeviceIDFetcher::GetMachineID() {
#if defined(OS_WIN) && defined(ENABLE_RLZ)
  std::string result;
  rlz_lib::GetMachineId(&result);
  return result;
#elif defined(OS_CHROMEOS)
  chromeos::CryptohomeLibrary* c_home = chromeos::CryptohomeLibrary::Get();
  return c_home->GetSystemSalt();
#else
  // Not implemented for other platforms.
  NOTREACHED();
  return "";
#endif
}

}  // namespace chrome
