// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_plugin_util.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"

//
// ScopableCPRequest
//

ScopableCPRequest::ScopableCPRequest(const char* u, const char* m,
                                     CPBrowsingContext c) {
  pdata = NULL;
  data = NULL;
#if defined(OS_WIN)
  url = _strdup(u);
  method = _strdup(m);
#else
  url = strdup(u);
  method = strdup(m);
#endif
  context = c;
}

ScopableCPRequest::~ScopableCPRequest() {
  pdata = NULL;
  data = NULL;
  free(const_cast<char*>(url));
  free(const_cast<char*>(method));
}

//
// PluginHelper
//

// static
void PluginHelper::DestroyAllHelpersForPlugin(ChromePluginLib* plugin) {
  NotificationService::current()->Notify(
      NotificationType::CHROME_PLUGIN_UNLOADED,
      Source<ChromePluginLib>(plugin),
      NotificationService::NoDetails());
}

PluginHelper::PluginHelper(ChromePluginLib* plugin) : plugin_(plugin) {
  DCHECK(CalledOnValidThread());
  registrar_.Add(this, NotificationType::CHROME_PLUGIN_UNLOADED,
                 Source<ChromePluginLib>(plugin_));
}

PluginHelper::~PluginHelper() {
  DCHECK(CalledOnValidThread());
}

void PluginHelper::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  DCHECK(CalledOnValidThread());
  DCHECK(type == NotificationType::CHROME_PLUGIN_UNLOADED);
  DCHECK(plugin_ == Source<ChromePluginLib>(source).ptr());

  delete this;
}

//
// PluginResponseUtils
//

uint32 PluginResponseUtils::CPLoadFlagsToNetFlags(uint32 flags) {
  uint32 net_flags = 0;
#define HANDLE_FLAG(name) \
  if (flags & CPREQUEST##name) \
  net_flags |= net::name

  HANDLE_FLAG(LOAD_VALIDATE_CACHE);
  HANDLE_FLAG(LOAD_BYPASS_CACHE);
  HANDLE_FLAG(LOAD_PREFERRING_CACHE);
  HANDLE_FLAG(LOAD_ONLY_FROM_CACHE);
  HANDLE_FLAG(LOAD_DISABLE_CACHE);
  HANDLE_FLAG(LOAD_DISABLE_INTERCEPT);

  net_flags |= net::LOAD_ENABLE_UPLOAD_PROGRESS;

  return net_flags;
}

int PluginResponseUtils::GetResponseInfo(
    const net::HttpResponseHeaders* response_headers,
    CPResponseInfoType type, void* buf, size_t buf_size) {
  if (!response_headers)
    return CPERR_FAILURE;

  switch (type) {
  case CPRESPONSEINFO_HTTP_STATUS:
    if (buf && buf_size) {
      int status = response_headers->response_code();
      memcpy(buf, &status, std::min(buf_size, sizeof(status)));
    }
    break;
  case CPRESPONSEINFO_HTTP_RAW_HEADERS: {
    const std::string& headers = response_headers->raw_headers();
    if (buf_size < headers.size()+1)
      return static_cast<int>(headers.size()+1);
    if (buf)
      memcpy(buf, headers.c_str(), headers.size()+1);
    break;
    }
  default:
    return CPERR_INVALID_VERSION;
  }

  return CPERR_SUCCESS;
}

CPError CPB_GetCommandLineArgumentsCommon(const char* url,
                                          std::string* arguments) {
  // TODO(aa): all of this code is only used by Gears, which we are removing.
#if defined(OS_WIN)
  const CommandLine cmd = *CommandLine::ForCurrentProcess();
  std::wstring arguments_w;

  // Use the same UserDataDir for new launches that we currently have set.
  FilePath user_data_dir = cmd.GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty()) {
    // Make sure user_data_dir is an absolute path.
    if (file_util::AbsolutePath(&user_data_dir) &&
        file_util::PathExists(user_data_dir)) {
      // TODO(evanm): use CommandLine APIs instead of this.
      arguments_w += std::wstring(L"--") + ASCIIToWide(switches::kUserDataDir) +
                     L"=\"" + user_data_dir.value() + L"\" ";
    }
  }

  // Use '--app=url' instead of just 'url' to launch the browser with minimal
  // chrome.
  // Note: Do not change this flag!  Old Gears shortcuts will break if you do!
  std::string url_string(url);
  ReplaceSubstringsAfterOffset(&url_string, 0, "\\", "%5C");
  ReplaceSubstringsAfterOffset(&url_string, 0, "\"", "%22");
  ReplaceSubstringsAfterOffset(&url_string, 0, ";",  "%3B");
  ReplaceSubstringsAfterOffset(&url_string, 0, "$",  "%24");
  // Windows shortcuts can't escape % so we use \x instead.
  ReplaceSubstringsAfterOffset(&url_string, 0, "%",  "\\x");
  std::wstring url_w = UTF8ToWide(url_string);
  // TODO(evanm): use CommandLine APIs instead of this.
  arguments_w += std::wstring(L"--") + ASCIIToWide(switches::kApp) +
      L"=\"" + url_w + L"\"";

  *arguments = WideToUTF8(arguments_w);
#else
  // None of this code is used on non-Windows platforms.
  NOTREACHED();
#endif

  return CPERR_SUCCESS;
}

//
// Host functions shared by browser and plugin processes
//

void* STDCALL CPB_Alloc(uint32 size) {
  return malloc(size);
}

void STDCALL CPB_Free(void* memory) {
  free(memory);
}
