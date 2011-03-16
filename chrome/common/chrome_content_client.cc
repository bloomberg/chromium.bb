// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include "chrome/common/child_process_logging.h"
#include "chrome/common/default_plugin.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace chrome {

void ChromeContentClient::SetActiveURL(const GURL& url) {
  child_process_logging::SetActiveURL(url);
}

void ChromeContentClient::SetGpuInfo(const GPUInfo& gpu_info) {
  child_process_logging::SetGpuInfo(gpu_info);
}

void ChromeContentClient::PluginProcessStarted(const string16& plugin_name) {
#if defined(OS_MACOSX)
  base::mac::ScopedCFTypeRef<CFStringRef> cf_plugin_name(
      base::SysUTF16ToCFStringRef(plugin_name));
  base::mac::ScopedCFTypeRef<CFStringRef> app_name(
      base::SysUTF16ToCFStringRef(
          l10n_util::GetStringUTF16(IDS_SHORT_PLUGIN_APP_NAME)));
  base::mac::ScopedCFTypeRef<CFStringRef> process_name(
      CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@ (%@)"),
                               cf_plugin_name.get(), app_name.get()));
  base::mac::SetProcessName(process_name);
#endif

#if !defined(NACL_WIN64)  // We don't link this in the NaCl 64 bit binary.
  chrome::RegisterInternalDefaultPlugin();
#endif
}

}  // namespace chrome
