// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/ie_importer_test_registry_overrider_win.h"

#include <windows.h>

#include <string>

#include "base/environment.h"
#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"

namespace {

// The key to which a random subkey will be appended. This key itself will never
// be deleted.
const wchar_t kTestHKCUOverrideKey[] = L"SOFTWARE\\Chromium Unit Tests";
const char kTestHKCUOverrideEnvironmentVariable[] =
    "IE_IMPORTER_TEST_OVERRIDE_HKCU";

base::string16 GetTestKeyFromSubKey(const base::string16& subkey) {
  base::string16 key(kTestHKCUOverrideKey);
  key.push_back(L'\\');
  key.append(subkey);
  return key;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// IEImporterTestRegistryOverrider, public:

IEImporterTestRegistryOverrider::IEImporterTestRegistryOverrider()
    : override_performed_(false), override_initiated_(false) {
}

IEImporterTestRegistryOverrider::~IEImporterTestRegistryOverrider() {
  base::AutoLock auto_lock(lock_.Get());
  if (override_performed_) {
    override_active_in_process_ = false;

    DWORD result = RegOverridePredefKey(HKEY_CURRENT_USER, NULL);
    DLOG_IF(ERROR, result != ERROR_SUCCESS)
        << "Failed to unset current override";

    if (override_initiated_) {
      base::string16 subkey;
      if (!GetSubKeyFromEnvironment(&subkey)) {
        NOTREACHED();
      } else {
        base::string16 key(GetTestKeyFromSubKey(subkey));
        base::win::RegKey reg_key(HKEY_CURRENT_USER, key.c_str(),
                                  KEY_ALL_ACCESS);
        DCHECK(reg_key.Valid());
        reg_key.DeleteKey(L"");
      }

      scoped_ptr<base::Environment> env(base::Environment::Create());
      env->UnSetVar(kTestHKCUOverrideEnvironmentVariable);
    }
  }
}

bool IEImporterTestRegistryOverrider::SetRegistryOverride() {
  base::AutoLock auto_lock(lock_.Get());
  DCHECK(!GetSubKeyFromEnvironment(NULL));
  override_initiated_ = true;
  scoped_ptr<base::Environment> env(base::Environment::Create());
  const base::string16 random_subkey(UTF8ToUTF16(base::GenerateGUID()));
  return StartRegistryOverride(random_subkey) &&
      env->SetVar(kTestHKCUOverrideEnvironmentVariable,
                  UTF16ToUTF8(random_subkey));
}

bool IEImporterTestRegistryOverrider::StartRegistryOverrideIfNeeded() {
  base::AutoLock auto_lock(lock_.Get());
  base::string16 subkey;
  if (!GetSubKeyFromEnvironment(&subkey))
    return true;
  return StartRegistryOverride(subkey);
}

////////////////////////////////////////////////////////////////////////////////
// IEImporterTestRegistryOverrider, private:

bool IEImporterTestRegistryOverrider::StartRegistryOverride(
    const base::string16& subkey) {
  lock_.Get().AssertAcquired();
  // Override the registry whether |override_active_in_process_| or not as some
  // Windows API calls seem to reset the override (IUrlHistoryStg2::AddUrl is
  // suspected on Win7-). However, make sure to first undo any existing override
  // as this method will otherwise generate a fake key within the current fake
  // key.
  DWORD result = RegOverridePredefKey(HKEY_CURRENT_USER, NULL);
  DLOG_IF(ERROR, result != ERROR_SUCCESS) << "Failed to unset current override";

  const base::string16 key(GetTestKeyFromSubKey(subkey));
  base::win::RegKey temp_hkcu_hive_key;
  result = temp_hkcu_hive_key.Create(HKEY_CURRENT_USER, key.c_str(),
                                     KEY_ALL_ACCESS);
  if (result != ERROR_SUCCESS || !temp_hkcu_hive_key.Valid()) {
    DPLOG(ERROR) << result;
    return false;
  }

  if (!override_active_in_process_) {
    // Only take ownership of the override if it wasn't already set in this
    // process.
    override_performed_ = true;
    override_active_in_process_ = true;
  }
  return RegOverridePredefKey(HKEY_CURRENT_USER,
                              temp_hkcu_hive_key.Handle()) == ERROR_SUCCESS;
}

bool IEImporterTestRegistryOverrider::GetSubKeyFromEnvironment(
    base::string16* subkey) {
  lock_.Get().AssertAcquired();
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string value;
  bool result = env->GetVar(kTestHKCUOverrideEnvironmentVariable, &value);
  if (result && subkey)
    *subkey = UTF8ToUTF16(value);
  return result;
}

// static
bool IEImporterTestRegistryOverrider::override_active_in_process_ = false;

// static
base::LazyInstance<base::Lock>::Leaky IEImporterTestRegistryOverrider::lock_ =
    LAZY_INSTANCE_INITIALIZER;
