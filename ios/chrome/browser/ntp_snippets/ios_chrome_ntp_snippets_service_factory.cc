// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_snippets/ios_chrome_ntp_snippets_service_factory.h"

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/image_fetcher/image_fetcher.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/ntp_snippets/ntp_snippets_fetcher.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/suggestions/image_fetcher_impl.h"
#include "ios/chrome/browser/suggestions/suggestions_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_request_context_getter.h"

using suggestions::ImageFetcherImpl;
using suggestions::SuggestionsService;
using suggestions::SuggestionsServiceFactory;

namespace {

void ParseJson(
    const std::string& json,
    const ntp_snippets::NTPSnippetsFetcher::SuccessCallback& success_callback,
    const ntp_snippets::NTPSnippetsFetcher::ErrorCallback& error_callback) {
  base::JSONReader json_reader;
  std::unique_ptr<base::Value> value = json_reader.ReadToValue(json);
  if (value) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(success_callback, base::Passed(&value)));
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback, json_reader.GetErrorMessage()));
  }
}

}  // namespace

// static
IOSChromeNTPSnippetsServiceFactory*
IOSChromeNTPSnippetsServiceFactory::GetInstance() {
  return base::Singleton<IOSChromeNTPSnippetsServiceFactory>::get();
}

// static
ntp_snippets::NTPSnippetsService*
IOSChromeNTPSnippetsServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());
  return static_cast<ntp_snippets::NTPSnippetsService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

IOSChromeNTPSnippetsServiceFactory::IOSChromeNTPSnippetsServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "NTPSnippetsService",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(SuggestionsServiceFactory::GetInstance());
}

IOSChromeNTPSnippetsServiceFactory::~IOSChromeNTPSnippetsServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromeNTPSnippetsServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* browser_state) const {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(browser_state);
  DCHECK(!browser_state->IsOffTheRecord());
  scoped_refptr<net::URLRequestContextGetter> request_context =
      browser_state->GetRequestContext();
  SuggestionsService* suggestions_service =
      SuggestionsServiceFactory::GetForBrowserState(chrome_browser_state);

  ntp_snippets::NTPSnippetsScheduler* scheduler = nullptr;

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      web::WebThread::GetBlockingPool()
          ->GetSequencedTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::GetSequenceToken(),
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  return base::WrapUnique(new ntp_snippets::NTPSnippetsService(
      chrome_browser_state->GetPrefs(), suggestions_service, task_runner,
      GetApplicationContext()->GetApplicationLocale(), scheduler,
      base::WrapUnique(new ntp_snippets::NTPSnippetsFetcher(
          request_context, base::Bind(&ParseJson),
          GetChannel() == version_info::Channel::STABLE)),
      base::WrapUnique(new ImageFetcherImpl(
          request_context.get(), web::WebThread::GetBlockingPool()))));
}
