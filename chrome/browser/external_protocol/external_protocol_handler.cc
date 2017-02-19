// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include <stddef.h>

#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "url/gurl.h"

using content::BrowserThread;

// Whether we accept requests for launching external protocols. This is set to
// false every time an external protocol is requested, and set back to true on
// each user gesture. This variable should only be accessed from the UI thread.
static bool g_accept_requests = true;

namespace {

// Functions enabling unit testing. Using a NULL delegate will use the default
// behavior; if a delegate is provided it will be used instead.
scoped_refptr<shell_integration::DefaultProtocolClientWorker> CreateShellWorker(
    const shell_integration::DefaultWebClientWorkerCallback& callback,
    const std::string& protocol,
    ExternalProtocolHandler::Delegate* delegate) {
  if (delegate)
    return delegate->CreateShellWorker(callback, protocol);

  return new shell_integration::DefaultProtocolClientWorker(callback, protocol);
}

ExternalProtocolHandler::BlockState GetBlockStateWithDelegate(
    const std::string& scheme,
    ExternalProtocolHandler::Delegate* delegate,
    Profile* profile) {
  if (!delegate)
    return ExternalProtocolHandler::GetBlockState(scheme, profile);

  return delegate->GetBlockState(scheme, profile);
}

void RunExternalProtocolDialogWithDelegate(
    const GURL& url,
    int render_process_host_id,
    int routing_id,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    ExternalProtocolHandler::Delegate* delegate) {
  if (!delegate) {
    ExternalProtocolHandler::RunExternalProtocolDialog(
        url, render_process_host_id, routing_id, page_transition,
        has_user_gesture);
  } else {
    delegate->RunExternalProtocolDialog(
        url, render_process_host_id, routing_id, page_transition,
        has_user_gesture);
  }
}

void LaunchUrlWithoutSecurityCheckWithDelegate(
    const GURL& url,
    int render_process_host_id,
    int render_view_routing_id,
    ExternalProtocolHandler::Delegate* delegate) {
  content::WebContents* web_contents = tab_util::GetWebContentsByID(
      render_process_host_id, render_view_routing_id);

  if (!delegate) {
    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(url, web_contents);
  } else {
    delegate->LaunchUrlWithoutSecurityCheck(url, web_contents);
  }
}

// When we are about to launch a URL with the default OS level application, we
// check if the external application will be us. If it is we just ignore the
// request.
void OnDefaultProtocolClientWorkerFinished(
    const GURL& escaped_url,
    int render_process_host_id,
    int render_view_routing_id,
    bool prompt_user,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    ExternalProtocolHandler::Delegate* delegate,
    shell_integration::DefaultWebClientState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (delegate)
    delegate->FinishedProcessingCheck();

  if (state == shell_integration::IS_DEFAULT) {
    if (delegate)
      delegate->BlockRequest();
    return;
  }

  // If we get here, either we are not the default or we cannot work out
  // what the default is, so we proceed.
  if (prompt_user) {
    // Ask the user if they want to allow the protocol. This will call
    // LaunchUrlWithoutSecurityCheck if the user decides to accept the
    // protocol.
    RunExternalProtocolDialogWithDelegate(
        escaped_url, render_process_host_id, render_view_routing_id,
        page_transition, has_user_gesture, delegate);
    return;
  }

  LaunchUrlWithoutSecurityCheckWithDelegate(escaped_url, render_process_host_id,
                                            render_view_routing_id, delegate);
}

}  // namespace

const char ExternalProtocolHandler::kRememberCheckboxMetric[] =
    "BrowserDialogs.ExternalProtocol.RememberCheckbox";
const char ExternalProtocolHandler::kHandleStateMetric[] =
    "BrowserDialogs.ExternalProtocol.HandleState";

// static
ExternalProtocolHandler::BlockState ExternalProtocolHandler::GetBlockState(
    const std::string& scheme,
    Profile* profile) {
  // If we are being carpet bombed, block the request.
  if (!g_accept_requests)
    return BLOCK;

  if (scheme.length() == 1) {
    // We have a URL that looks something like:
    //   C:/WINDOWS/system32/notepad.exe
    // ShellExecuting this URL will cause the specified program to be executed.
    return BLOCK;
  }

  // Check if there are any prefs in the local state. If there are, wipe them,
  // and migrate the prefs to the profile.
  // TODO(ramyasharma) remove the migration in M61.
  PrefService* local_prefs = g_browser_process->local_state();
  PrefService* profile_prefs = profile->GetPrefs();
  if (local_prefs && profile_prefs) {  // May be NULL during testing.
    DictionaryPrefUpdate local_state_schemas(local_prefs,
                                             prefs::kExcludedSchemes);
    DictionaryPrefUpdate update_excluded_schemas_profile(
        profile_prefs, prefs::kExcludedSchemes);
    if (update_excluded_schemas_profile->empty()) {
      // Copy local state to profile state.
      for (base::DictionaryValue::Iterator it(*local_state_schemas);
           !it.IsAtEnd(); it.Advance()) {
        bool is_blocked;
        // Discard local state if set to blocked, to reset all users
        // stuck in 'Do Nothing' + 'Do Not Open' state back to the default
        // prompt state.
        if (it.value().GetAsBoolean(&is_blocked) && !is_blocked)
          update_excluded_schemas_profile->SetBoolean(it.key(), is_blocked);
      }
      // TODO(ramyasharma): Clear only if required.
      local_prefs->ClearPref(prefs::kExcludedSchemes);
    }

    // Prepopulate the default states each time.
    PrepopulateDictionary(update_excluded_schemas_profile.Get());

    bool should_block;
    if (update_excluded_schemas_profile->GetBoolean(scheme, &should_block))
      return should_block ? BLOCK : DONT_BLOCK;
  }

  return UNKNOWN;
}

// static
void ExternalProtocolHandler::SetBlockState(const std::string& scheme,
                                            BlockState state,
                                            Profile* profile) {
  // Set in the stored prefs.
  PrefService* profile_prefs = profile->GetPrefs();
  if (profile_prefs) {  // May be NULL during testing.
    DictionaryPrefUpdate update_excluded_schemas_profile(
        profile_prefs, prefs::kExcludedSchemes);
    if (!update_excluded_schemas_profile->empty()) {
      if (state == UNKNOWN)
        update_excluded_schemas_profile->Remove(scheme, nullptr);
      else
        update_excluded_schemas_profile->SetBoolean(scheme, (state == BLOCK));
    }
  }
}

// static
void ExternalProtocolHandler::LaunchUrlWithDelegate(
    const GURL& url,
    int render_process_host_id,
    int render_view_routing_id,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    Delegate* delegate) {
  DCHECK(base::MessageLoopForUI::IsCurrent());

  // Escape the input scheme to be sure that the command does not
  // have parameters unexpected by the external program.
  std::string escaped_url_string = net::EscapeExternalHandlerValue(url.spec());
  GURL escaped_url(escaped_url_string);

  content::WebContents* web_contents = tab_util::GetWebContentsByID(
      render_process_host_id, render_view_routing_id);
  Profile* profile = nullptr;
  if (web_contents)  // Maybe NULL during testing.
    profile = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  BlockState block_state =
      GetBlockStateWithDelegate(escaped_url.scheme(), delegate, profile);
  if (block_state == BLOCK) {
    if (delegate)
      delegate->BlockRequest();
    return;
  }

  g_accept_requests = false;

  // The worker creates tasks with references to itself and puts them into
  // message loops.
  shell_integration::DefaultWebClientWorkerCallback callback = base::Bind(
      &OnDefaultProtocolClientWorkerFinished, url, render_process_host_id,
      render_view_routing_id, block_state == UNKNOWN, page_transition,
      has_user_gesture, delegate);

  // Start the check process running. This will send tasks to the FILE thread
  // and when the answer is known will send the result back to
  // OnDefaultProtocolClientWorkerFinished().
  CreateShellWorker(callback, escaped_url.scheme(), delegate)
      ->StartCheckIsDefault();
}

// static
void ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
    const GURL& url,
    content::WebContents* web_contents) {
  // |web_contents| is only passed in to find browser context. Do not assume
  // that the external protocol request came from the main frame.
  if (!web_contents)
    return;

  platform_util::OpenExternal(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()), url);
}

// static
void ExternalProtocolHandler::PermitLaunchUrl() {
  DCHECK(base::MessageLoopForUI::IsCurrent());
  g_accept_requests = true;
}

// static
void ExternalProtocolHandler::PrepopulateDictionary(
    base::DictionaryValue* win_pref) {
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
void ExternalProtocolHandler::RecordCheckboxStateMetrics(bool selected) {
  UMA_HISTOGRAM_BOOLEAN(kRememberCheckboxMetric, selected);
}

// static
void ExternalProtocolHandler::RecordHandleStateMetrics(bool checkbox_selected,
                                                       BlockState block_state) {
  HandleState handle_state = DONT_LAUNCH;
  switch (block_state) {
    case DONT_BLOCK:
      handle_state = checkbox_selected ? CHECKED_LAUNCH : LAUNCH;
      break;
    case BLOCK:
      handle_state = checkbox_selected ? CHECKED_DONT_LAUNCH : DONT_LAUNCH;
      break;
    case UNKNOWN:
      NOTREACHED();
      return;
  }
  UMA_HISTOGRAM_ENUMERATION(kHandleStateMetric, handle_state,
                            HANDLE_STATE_LAST);
}

// static
void ExternalProtocolHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kExcludedSchemes);
}

// static
void ExternalProtocolHandler::ClearData(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  prefs->ClearPref(prefs::kExcludedSchemes);
}
