// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// KeywordExtensionsDelegateImpl contains the extensions-only logic used by
// KeywordProvider. Overrides KeywordExtensionsDelegate which does nothing.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_EXTENSIONS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_EXTENSIONS_DELEGATE_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/keyword_extensions_delegate.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "components/autocomplete/autocomplete_input.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

#if !defined(ENABLE_EXTENSIONS)
#error "Should not be included when extensions are disabled"
#endif

class KeywordExtensionsDelegateImpl : public KeywordExtensionsDelegate,
                                      public content::NotificationObserver {
 public:
  explicit KeywordExtensionsDelegateImpl(KeywordProvider* provider);
  virtual ~KeywordExtensionsDelegateImpl();

 private:
  // KeywordExtensionsDelegate:
  virtual void IncrementInputId() OVERRIDE;
  virtual bool IsEnabledExtension(Profile* profile,
                                  const std::string& extension_id) OVERRIDE;
  virtual bool Start(const AutocompleteInput& input,
                     bool minimal_changes,
                     const TemplateURL* template_url,
                     const base::string16& remaining_input) OVERRIDE;
  virtual void EnterExtensionKeywordMode(
      const std::string& extension_id) OVERRIDE;
  virtual void MaybeEndExtensionKeywordMode() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* profile() { return provider_->profile_; }
  ACMatches* matches() { return &provider_->matches_; }
  void set_done(bool done) {
    provider_->done_ = done;
  }

  // Notifies the KeywordProvider about asynchronous updates from the extension.
  void OnProviderUpdate(bool updated_matches);

  // Identifies the current input state. This is incremented each time the
  // autocomplete edit's input changes in any way. It is used to tell whether
  // suggest results from the extension are current.
  int current_input_id_;

  // The input state at the time we last asked the extension for suggest
  // results.
  AutocompleteInput extension_suggest_last_input_;

  // We remember the last suggestions we've received from the extension in case
  // we need to reset our matches without asking the extension again.
  std::vector<AutocompleteMatch> extension_suggest_matches_;

  // If non-empty, holds the ID of the extension whose keyword is currently in
  // the URL bar while the autocomplete popup is open.
  std::string current_keyword_extension_id_;

  // The owner of this class.
  KeywordProvider* provider_;

  content::NotificationRegistrar registrar_;

  // We need our input IDs to be unique across all profiles, so we keep a global
  // UID that each provider uses.
  static int global_input_uid_;

  DISALLOW_COPY_AND_ASSIGN(KeywordExtensionsDelegateImpl);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_KEYWORD_EXTENSIONS_DELEGATE_IMPL_H_
