// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_IMPL_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_IMPL_H_

#include "base/macros.h"
#include "components/spellcheck/browser/spelling_service_client.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/spellcheck_build_features.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

#if !BUILDFLAG(ENABLE_SPELLCHECK)
#error "Spellcheck should be enabled."
#endif

class SpellcheckCustomDictionary;
class SpellcheckService;

struct SpellCheckResult;

class SpellCheckHostImpl : public spellcheck::mojom::SpellCheckHost {
 public:
  explicit SpellCheckHostImpl(
      const service_manager::Identity& renderer_identity);
  ~SpellCheckHostImpl() override;

  static void Create(spellcheck::mojom::SpellCheckHostRequest request,
                     const service_manager::BindSourceInfo& source_info);

 private:
  friend class TestSpellCheckHostImpl;

  // spellcheck::mojom::SpellCheckHost:
  void RequestDictionary() override;
  void NotifyChecked(const base::string16& word, bool misspelled) override;
  void CallSpellingService(const base::string16& text,
                           CallSpellingServiceCallback callback) override;

#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  // Invoked when the remote Spelling service has finished checking the
  // text of a CallSpellingService request.
  void CallSpellingServiceDone(
      CallSpellingServiceCallback callback,
      bool success,
      const base::string16& text,
      const std::vector<SpellCheckResult>& service_results) const;

  // Filter out spelling corrections of custom dictionary words from the
  // Spelling service results.
  static std::vector<SpellCheckResult> FilterCustomWordResults(
      const std::string& text,
      const SpellcheckCustomDictionary& custom_dictionary,
      const std::vector<SpellCheckResult>& service_results);
#endif

  // Returns the SpellcheckService of our |render_process_id_|. The return
  // is null if the render process is being shut down.
  virtual SpellcheckService* GetSpellcheckService() const;

  // The identity of the renderer service.
  const service_manager::Identity renderer_identity_;

  // A JSON-RPC client that calls the remote Spelling service.
  SpellingServiceClient client_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckHostImpl);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELL_CHECK_HOST_IMPL_H_
