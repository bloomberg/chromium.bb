// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include <set>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/escape.h"

using content::BrowserThread;

// Whether we accept requests for launching external protocols. This is set to
// false every time an external protocol is requested, and set back to true on
// each user gesture. This variable should only be accessed from the UI thread.
static bool g_accept_requests = true;

namespace {

// Functions enabling unit testing. Using a NULL delegate will use the default
// behavior; if a delegate is provided it will be used instead.
ShellIntegration::DefaultProtocolClientWorker* CreateShellWorker(
    ShellIntegration::DefaultWebClientObserver* observer,
    const std::string& protocol,
    ExternalProtocolHandler::Delegate* delegate) {
  if (!delegate)
    return new ShellIntegration::DefaultProtocolClientWorker(observer,
                                                             protocol);

  return delegate->CreateShellWorker(observer, protocol);
}

ExternalProtocolHandler::BlockState GetBlockStateWithDelegate(
    const std::string& scheme,
    ExternalProtocolHandler::Delegate* delegate) {
  if (!delegate)
    return ExternalProtocolHandler::GetBlockState(scheme);

  return delegate->GetBlockState(scheme);
}

void RunExternalProtocolDialogWithDelegate(
    const GURL& url,
    int render_process_host_id,
    int routing_id,
    ExternalProtocolHandler::Delegate* delegate) {
  if (!delegate) {
    ExternalProtocolHandler::RunExternalProtocolDialog(url,
                                                       render_process_host_id,
                                                       routing_id);
  } else {
    delegate->RunExternalProtocolDialog(url, render_process_host_id,
                                        routing_id);
  }
}

void LaunchUrlWithoutSecurityCheckWithDelegate(
    const GURL& url,
    ExternalProtocolHandler::Delegate* delegate) {
  if (!delegate)
    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(url);
  else
    delegate->LaunchUrlWithoutSecurityCheck(url);
}

// When we are about to launch a URL with the default OS level application,
// we check if that external application will be us. If it is we just ignore
// the request.
class ExternalDefaultProtocolObserver
    : public ShellIntegration::DefaultWebClientObserver {
 public:
  ExternalDefaultProtocolObserver(const GURL& escaped_url,
                                  int render_process_host_id,
                                  int tab_contents_id,
                                  bool prompt_user,
                                  ExternalProtocolHandler::Delegate* delegate)
      : delegate_(delegate),
        escaped_url_(escaped_url),
        render_process_host_id_(render_process_host_id),
        tab_contents_id_(tab_contents_id),
        prompt_user_(prompt_user) {}

  virtual void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) OVERRIDE {
    DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());

    // If we are still working out if we're the default, or we've found
    // out we definately are the default, we end here.
    if (state == ShellIntegration::STATE_PROCESSING) {
      return;
    }

    if (delegate_)
      delegate_->FinishedProcessingCheck();

    if (state == ShellIntegration::STATE_IS_DEFAULT) {
      if (delegate_)
        delegate_->BlockRequest();
      return;
    }

    // If we get here, either we are not the default or we cannot work out
    // what the default is, so we proceed.
    if (prompt_user_) {
      // Ask the user if they want to allow the protocol. This will call
      // LaunchUrlWithoutSecurityCheck if the user decides to accept the
      // protocol.
      RunExternalProtocolDialogWithDelegate(escaped_url_,
          render_process_host_id_, tab_contents_id_, delegate_);
      return;
    }

    LaunchUrlWithoutSecurityCheckWithDelegate(escaped_url_, delegate_);
  }

  virtual bool IsOwnedByWorker() OVERRIDE { return true; }

 private:
  ExternalProtocolHandler::Delegate* delegate_;
  GURL escaped_url_;
  int render_process_host_id_;
  int tab_contents_id_;
  bool prompt_user_;
};

}  // namespace

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
#if defined(OS_WIN)
    "ms-windows-store",
#endif
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
      update_excluded_schemas->SetBoolean(scheme, (state == BLOCK));
    }
  }
}

// static
void ExternalProtocolHandler::LaunchUrlWithDelegate(const GURL& url,
                                                    int render_process_host_id,
                                                    int tab_contents_id,
                                                    Delegate* delegate) {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());

  // Escape the input scheme to be sure that the command does not
  // have parameters unexpected by the external program.
  std::string escaped_url_string = net::EscapeExternalHandlerValue(url.spec());
  GURL escaped_url(escaped_url_string);
  BlockState block_state = GetBlockStateWithDelegate(escaped_url.scheme(),
                                                     delegate);
  if (block_state == BLOCK) {
    if (delegate)
      delegate->BlockRequest();
    return;
  }

  g_accept_requests = false;

  // The worker creates tasks with references to itself and puts them into
  // message loops. When no tasks are left it will delete the observer and
  // eventually be deleted itself.
  ShellIntegration::DefaultWebClientObserver* observer =
      new ExternalDefaultProtocolObserver(url,
                                          render_process_host_id,
                                          tab_contents_id,
                                          block_state == UNKNOWN,
                                          delegate);
  scoped_refptr<ShellIntegration::DefaultProtocolClientWorker> worker =
      CreateShellWorker(observer, escaped_url.scheme(), delegate);

  // Start the check process running. This will send tasks to the FILE thread
  // and when the answer is known will send the result back to the observer on
  // the UI thread.
  worker->StartCheckIsDefault();
}

// static
void ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(const GURL& url) {
#if defined(OS_MACOSX)
  // This must run on the UI thread on OS X.
  platform_util::OpenExternal(url);
#else
  // Otherwise put this work on the file thread. On Windows ShellExecute may
  // block for a significant amount of time, and it shouldn't hurt on Linux.
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&platform_util::OpenExternal, url));
#endif
}

// static
void ExternalProtocolHandler::RegisterPrefs(PrefServiceSimple* prefs) {
  prefs->RegisterDictionaryPref(prefs::kExcludedSchemes);
}

// static
void ExternalProtocolHandler::PermitLaunchUrl() {
  DCHECK_EQ(MessageLoop::TYPE_UI, MessageLoop::current()->type());
  g_accept_requests = true;
}
