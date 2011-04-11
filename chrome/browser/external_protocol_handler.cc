// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol_handler.h"

#include <set>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

// Whether we accept requests for launching external protocols. This is set to
// false every time an external protocol is requested, and set back to true on
// each user gesture. This variable should only be accessed from the UI thread.
static bool g_accept_requests = true;

// static
void ExternalProtocolHandler::PrepopulateDictionary(DictionaryValue* win_pref) {
  static bool is_warm = false;
  if (is_warm)
    return;
  is_warm = true;

  static const char* const denied_schemes[] = {
    "afp",
    "data",
    "disk",
    "disks",
    // ShellExecuting file:///C:/WINDOWS/system32/notepad.exe will simply
    // execute the file specified!  Hopefully we won't see any "file" schemes
    // because we think of file:// URLs as handled URLs, but better to be safe
    // than to let an attacker format the user's hard drive.
    "file",
    "hcp",
    "javascript",
    "ms-help",
    "nntp",
    "shell",
    "vbscript",
    // view-source is a special case in chrome. When it comes through an
    // iframe or a redirect, it looks like an external protocol, but we don't
    // want to shellexecute it.
    "view-source",
    "vnd.ms.radio",
  };

  static const char* const allowed_schemes[] = {
    "mailto",
    "news",
    "snews",
  };

  bool should_block;
  for (size_t i = 0; i < arraysize(denied_schemes); ++i) {
    if (!win_pref->GetBoolean(denied_schemes[i], &should_block)) {
      win_pref->SetBoolean(denied_schemes[i], true);
    }
  }

  for (size_t i = 0; i < arraysize(allowed_schemes); ++i) {
    if (!win_pref->GetBoolean(allowed_schemes[i], &should_block)) {
      win_pref->SetBoolean(allowed_schemes[i], false);
    }
  }
}

// static
ExternalProtocolHandler::BlockState ExternalProtocolHandler::GetBlockState(
    const std::string& scheme) {
  // If we are being carpet bombed, block the request.
  if (!g_accept_requests)
    return BLOCK;

  if (scheme.length() == 1) {
    // We have a URL that looks something like:
    //   C:/WINDOWS/system32/notepad.exe
    // ShellExecuting this URL will cause the specified program to be executed.
    return BLOCK;
  }

  // Check the stored prefs.
  // TODO(pkasting): http://b/1119651 This kind of thing should go in the
  // preferences on the profile, not in the local state.
  PrefService* pref = g_browser_process->local_state();
  if (pref) {  // May be NULL during testing.
    DictionaryPrefUpdate update_excluded_schemas(pref, prefs::kExcludedSchemes);

    // Warm up the dictionary if needed.
    PrepopulateDictionary(update_excluded_schemas.Get());

    bool should_block;
    if (update_excluded_schemas->GetBoolean(scheme, &should_block))
      return should_block ? BLOCK : DONT_BLOCK;
  }

  return UNKNOWN;
}

// static
void ExternalProtocolHandler::SetBlockState(const std::string& scheme,
                                            BlockState state) {
  // Set in the stored prefs.
  // TODO(pkasting): http://b/1119651 This kind of thing should go in the
  // preferences on the profile, not in the local state.
  PrefService* pref = g_browser_process->local_state();
  if (pref) {  // May be NULL during testing.
    DictionaryPrefUpdate update_excluded_schemas(pref, prefs::kExcludedSchemes);

    if (state == UNKNOWN) {
      update_excluded_schemas->Remove(scheme, NULL);
    } else {
      update_excluded_schemas->SetBoolean(scheme,
                                          state == BLOCK ? true : false);
    }
  }
}

// static
void ExternalProtocolHandler::LaunchUrl(const GURL& url,
                                        int render_process_host_id,
                                        int tab_contents_id) {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());

  // Escape the input scheme to be sure that the command does not
  // have parameters unexpected by the external program.
  std::string escaped_url_string = EscapeExternalHandlerValue(url.spec());
  GURL escaped_url(escaped_url_string);
  BlockState block_state = GetBlockState(escaped_url.scheme());
  if (block_state == BLOCK)
    return;

  g_accept_requests = false;

  if (block_state == UNKNOWN) {
    // Ask the user if they want to allow the protocol. This will call
    // LaunchUrlWithoutSecurityCheck if the user decides to accept the protocol.
    RunExternalProtocolDialog(escaped_url,
                              render_process_host_id,
                              tab_contents_id);
    return;
  }

  LaunchUrlWithoutSecurityCheck(escaped_url);
}

// static
void ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(const GURL& url) {
#if defined(OS_MACOSX)
  // This must run on the UI thread on OS X.
  platform_util::OpenExternal(url);
#else
  // Otherwise put this work on the file thread. On Windows ShellExecute may
  // block for a significant amount of time, and it shouldn't hurt on Linux.
  MessageLoop* loop = g_browser_process->file_thread()->message_loop();
  if (loop == NULL) {
    return;
  }

  loop->PostTask(FROM_HERE,
      NewRunnableFunction(&platform_util::OpenExternal, url));
#endif
}

// static
void ExternalProtocolHandler::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kExcludedSchemes);
}

// static
void ExternalProtocolHandler::PermitLaunchUrl() {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  g_accept_requests = true;
}
