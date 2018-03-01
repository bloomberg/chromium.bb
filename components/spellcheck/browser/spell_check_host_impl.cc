// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/browser/spell_check_host_impl.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

SpellCheckHostImpl::SpellCheckHostImpl() = default;
SpellCheckHostImpl::~SpellCheckHostImpl() = default;

// static
void SpellCheckHostImpl::Create(
    spellcheck::mojom::SpellCheckHostRequest request) {
  mojo::MakeStrongBinding(std::make_unique<SpellCheckHostImpl>(),
                          std::move(request));
}

void SpellCheckHostImpl::RequestDictionary() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This API requires Chrome-only features.
  return;
}

void SpellCheckHostImpl::NotifyChecked(const base::string16& word,
                                       bool misspelled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This API requires Chrome-only features.
  return;
}

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellCheckHostImpl::CallSpellingService(
    const base::string16& text,
    CallSpellingServiceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (text.empty())
    mojo::ReportBadMessage(__FUNCTION__);

  // This API requires Chrome-only features.
  std::move(callback).Run(false, std::vector<SpellCheckResult>());
}
#endif  // !BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
void SpellCheckHostImpl::RequestTextCheck(const base::string16& text,
                                          int route_id,
                                          RequestTextCheckCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (text.empty())
    mojo::ReportBadMessage(__FUNCTION__);

#if defined(OS_ANDROID)
  session_bridge_.RequestTextCheck(text, std::move(callback));
#else
  // This API requires Chrome-only features on the platform.
  std::move(callback).Run(std::vector<SpellCheckResult>());
#endif
}

void SpellCheckHostImpl::ToggleSpellCheck(bool enabled, bool checked) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(OS_ANDROID)
  if (!enabled)
    session_bridge_.DisconnectSession();
#endif
}

void SpellCheckHostImpl::CheckSpelling(const base::string16& word,
                                       int route_id,
                                       CheckSpellingCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This API requires Chrome-only features.
  std::move(callback).Run(false);
}

void SpellCheckHostImpl::FillSuggestionList(
    const base::string16& word,
    FillSuggestionListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This API requires Chrome-only features.
  std::move(callback).Run(std::vector<base::string16>());
}
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)
