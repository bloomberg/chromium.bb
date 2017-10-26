// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_host_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

SpellCheckHostImpl::SpellCheckHostImpl(
    const service_manager::Identity& renderer_identity)
    : renderer_identity_(renderer_identity) {}

SpellCheckHostImpl::~SpellCheckHostImpl() = default;

// static
void SpellCheckHostImpl::Create(
    spellcheck::mojom::SpellCheckHostRequest request,
    const service_manager::BindSourceInfo& source_info) {
  mojo::MakeStrongBinding(
      base::MakeUnique<SpellCheckHostImpl>(source_info.identity),
      std::move(request));
}

void SpellCheckHostImpl::RequestDictionary() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The renderer has requested that we initialize its spellchecker. This
  // generally should only be called once per session, as after the first
  // call, future renderers will be passed the initialization information
  // on startup (or when the dictionary changes in some way).
  SpellcheckService* spellcheck = GetSpellcheckService();
  if (!spellcheck)
    return;  // Teardown.

  // The spellchecker initialization already started and finished; just
  // send it to the renderer.
  spellcheck->InitForRenderer(renderer_identity_);

  // TODO(rlp): Ensure that we do not initialize the hunspell dictionary
  // more than once if we get requests from different renderers.
}

void SpellCheckHostImpl::NotifyChecked(const base::string16& word,
                                       bool misspelled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SpellcheckService* spellcheck = GetSpellcheckService();
  if (!spellcheck)
    return;  // Teardown.
  if (spellcheck->GetMetrics())
    spellcheck->GetMetrics()->RecordCheckedWordStats(word, misspelled);
}

void SpellCheckHostImpl::CallSpellingService(
    const base::string16& text,
    CallSpellingServiceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (text.empty()) {
    std::move(callback).Run(false, std::vector<SpellCheckResult>());
    mojo::ReportBadMessage(__FUNCTION__);
    return;
  }

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // Checks the user profile and sends a JSON-RPC request to the Spelling
  // service if a user enables the "Ask Google for suggestions" option. When
  // a response is received (including an error) from the remote Spelling
  // service, calls CallSpellingServiceDone.
  content::BrowserContext* context =
      content::BrowserContext::GetBrowserContextForServiceUserId(
          renderer_identity_.user_id());
  client_.RequestTextCheck(
      context, SpellingServiceClient::SPELLCHECK, text,
      base::Bind(&SpellCheckHostImpl::CallSpellingServiceDone,
                 base::Unretained(this), base::Passed(&callback)));
#else
  std::move(callback).Run(false, std::vector<SpellCheckResult>());
#endif
}

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellCheckHostImpl::CallSpellingServiceDone(
    CallSpellingServiceCallback callback,
    bool success,
    const base::string16& text,
    const std::vector<SpellCheckResult>& service_results) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  SpellcheckService* spellcheck = GetSpellcheckService();
  if (!spellcheck) {  // Teardown.
    std::move(callback).Run(false, std::vector<SpellCheckResult>());
    return;
  }

  std::vector<SpellCheckResult> results = FilterCustomWordResults(
      base::UTF16ToUTF8(text), *spellcheck->GetCustomDictionary(),
      service_results);

  std::move(callback).Run(success, results);
}

// static
std::vector<SpellCheckResult> SpellCheckHostImpl::FilterCustomWordResults(
    const std::string& text,
    const SpellcheckCustomDictionary& custom_dictionary,
    const std::vector<SpellCheckResult>& service_results) {
  std::vector<SpellCheckResult> results;
  for (const auto& result : service_results) {
    const std::string word = text.substr(result.location, result.length);
    if (!custom_dictionary.HasWord(word))
      results.push_back(result);
  }

  return results;
}
#endif  // !BUILDFLAG(USE_BROWSER_SPELLCHECKER)

SpellcheckService* SpellCheckHostImpl::GetSpellcheckService() const {
  return SpellcheckServiceFactory::GetForRenderer(renderer_identity_);
}
