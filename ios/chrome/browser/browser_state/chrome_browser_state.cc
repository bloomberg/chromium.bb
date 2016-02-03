// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "components/prefs/json_pref_store.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/net/ios_chrome_url_request_context_getter.h"
#include "ios/chrome/browser/net/net_types.h"
#include "ios/public/provider/web/web_ui_ios.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_thread.h"
#include "ios/web/webui/url_data_manager_ios_backend.h"
#include "net/url_request/url_request_interceptor.h"

namespace ios {

ChromeBrowserState::ChromeBrowserState() {}

ChromeBrowserState::~ChromeBrowserState() {}

// static
ChromeBrowserState* ChromeBrowserState::FromBrowserState(
    web::BrowserState* browser_state) {
  // This is safe; this is the only implementation of BrowserState.
  return static_cast<ChromeBrowserState*>(browser_state);
}

// static
ChromeBrowserState* ChromeBrowserState::FromWebUIIOS(web::WebUIIOS* web_ui) {
  return FromBrowserState(web_ui->GetWebState()->GetBrowserState());
}

std::string ChromeBrowserState::GetDebugName() {
  // The debug name is based on the state path of the original browser state
  // to keep in sync with the meaning on other platforms.
  std::string name =
      GetOriginalChromeBrowserState()->GetStatePath().BaseName().MaybeAsASCII();
  if (name.empty()) {
    name = "UnknownBrowserState";
  }
  return name;
}

scoped_refptr<base::SequencedTaskRunner> ChromeBrowserState::GetIOTaskRunner() {
  base::FilePath browser_state_path =
      GetOriginalChromeBrowserState()->GetStatePath();
  return JsonPrefStore::GetTaskRunnerForFile(browser_state_path,
                                             web::WebThread::GetBlockingPool());
}

syncable_prefs::PrefServiceSyncable* ChromeBrowserState::GetSyncablePrefs() {
  return static_cast<syncable_prefs::PrefServiceSyncable*>(GetPrefs());
}

TestChromeBrowserState* ChromeBrowserState::AsTestChromeBrowserState() {
  return nullptr;
}

net::URLRequestContextGetter* ChromeBrowserState::GetRequestContext() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  if (!request_context_getter_) {
    ProtocolHandlerMap protocol_handlers;
    protocol_handlers[kChromeUIScheme] =
        linked_ptr<net::URLRequestJobFactory::ProtocolHandler>(
            web::URLDataManagerIOSBackend::CreateProtocolHandler(this)
                .release());
    URLRequestInterceptorScopedVector request_interceptors;
    request_context_getter_ = make_scoped_refptr(CreateRequestContext(
        &protocol_handlers, std::move(request_interceptors)));
  }
  return request_context_getter_.get();
}

}  // namespace ios
