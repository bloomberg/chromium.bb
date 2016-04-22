// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pepper_flash.h"

#include <stddef.h>

#include "base/strings/string_split.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/ppapi_utils.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#endif

namespace chrome {

const int32_t kPepperFlashPermissions =
    ppapi::PERMISSION_DEV | ppapi::PERMISSION_PRIVATE |
    ppapi::PERMISSION_BYPASS_USER_GESTURE | ppapi::PERMISSION_FLASH;
namespace {

// File name of the Pepper Flash component manifest on different platforms.
const char kPepperFlashManifestName[] = "Flapper";

// Name of the Pepper Flash OS in the component manifest.
const char kPepperFlashOperatingSystem[] =
#if defined(OS_MACOSX)
    "mac";
#elif defined(OS_WIN)
    "win";
#else  // OS_LINUX, etc. TODO(viettrungluu): Separate out Chrome OS and Android?
    "linux";
#endif

// Name of the Pepper Flash architecture in the component manifest.
const char kPepperFlashArch[] =
#if defined(ARCH_CPU_X86)
    "ia32";
#elif defined(ARCH_CPU_X86_64)
    "x64";
#else  // TODO(viettrungluu): Support an ARM check?
    "???";
#endif

// Returns true if the Pepper |interface_name| is implemented  by this browser.
// It does not check if the interface is proxied.
bool SupportsPepperInterface(const char* interface_name) {
  if (IsSupportedPepperInterface(interface_name))
    return true;
  // The PDF interface is invisible to SupportsInterface() on the browser
  // process because it is provided using PpapiInterfaceFactoryManager. We need
  // to check for that as well.
  // TODO(cpu): make this more sane.
  return (strcmp(interface_name, PPB_PDF_INTERFACE) == 0);
}

// Returns true if this browser implements one of the interfaces given in
// |interface_string|, which is a '|'-separated string of interface names.
bool CheckPepperFlashInterfaceString(const std::string& interface_string) {
  for (const std::string& name : base::SplitString(
           interface_string, "|",
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (SupportsPepperInterface(name.c_str()))
      return true;
  }
  return false;
}

// Returns true if this browser implements all the interfaces that Flash
// specifies in its component installer manifest.
bool CheckPepperFlashInterfaces(const base::DictionaryValue& manifest) {
  const base::ListValue* interface_list = NULL;

  // We don't *require* an interface list, apparently.
  if (!manifest.GetList("x-ppapi-required-interfaces", &interface_list))
    return true;

  for (size_t i = 0; i < interface_list->GetSize(); i++) {
    std::string interface_string;
    if (!interface_list->GetString(i, &interface_string))
      return false;
    if (!CheckPepperFlashInterfaceString(interface_string))
      return false;
  }

  return true;
}

}  // namespace

bool CheckPepperFlashManifest(const base::DictionaryValue& manifest,
                              Version* version_out) {
  std::string name;
  manifest.GetStringASCII("name", &name);
  // TODO(viettrungluu): Support WinFlapper for now, while we change the format
  // of the manifest. (Should be safe to remove checks for "WinFlapper" in, say,
  // Nov. 2011.)  crbug.com/98458
  if (name != kPepperFlashManifestName && name != "WinFlapper")
    return false;

  std::string proposed_version;
  manifest.GetStringASCII("version", &proposed_version);
  Version version(proposed_version.c_str());
  if (!version.IsValid())
    return false;

  if (!CheckPepperFlashInterfaces(manifest))
    return false;

  // TODO(viettrungluu): See above TODO.
  if (name == "WinFlapper") {
    *version_out = version;
    return true;
  }

  std::string os;
  manifest.GetStringASCII("x-ppapi-os", &os);
  if (os != kPepperFlashOperatingSystem)
    return false;

  std::string arch;
  manifest.GetStringASCII("x-ppapi-arch", &arch);
  if (arch != kPepperFlashArch)
    return false;

  *version_out = version;
  return true;
}

bool IsSystemFlashScriptDebuggerPresent() {
#if defined(OS_WIN)
  const wchar_t kFlashRegistryRoot[] =
      L"SOFTWARE\\Macromedia\\FlashPlayerPepper";
  const wchar_t kIsDebuggerValueName[] = L"isScriptDebugger";

  base::win::RegKey path_key(HKEY_LOCAL_MACHINE, kFlashRegistryRoot, KEY_READ);
  DWORD debug_value;
  if (path_key.ReadValueDW(kIsDebuggerValueName, &debug_value) != ERROR_SUCCESS)
    return false;

  return (debug_value == 1);
#else
  // TODO(wfh): implement this on OS X and Linux. crbug.com/497996.
  return false;
#endif
}

}  // namespace chrome
