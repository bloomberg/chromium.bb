// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_host_impl.h"

#include <set>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_platform_mac.h"
#include "chrome/browser/spellchecker/spellcheck_profile_provider.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/hunspell/google/bdict.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

// Constructed on UI thread.
SpellCheckHostImpl::SpellCheckHostImpl(
    SpellCheckProfileProvider* profile,
    const std::string& language,
    net::URLRequestContextGetter* request_context_getter,
    SpellCheckHostMetrics* metrics)
    : profile_(profile),
      metrics_(metrics),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      hunspell_dictionary_(new SpellcheckHunspellDictionary(
          NULL, language, request_context_getter, this)) {
  DCHECK(profile_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(weak_ptr_factory_.GetWeakPtr(),
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
}

SpellCheckHostImpl::~SpellCheckHostImpl() {
}

void SpellCheckHostImpl::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  hunspell_dictionary_->Initialize();
}

void SpellCheckHostImpl::UnsetProfile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  profile_ = NULL;
  registrar_.RemoveAll();
}

void SpellCheckHostImpl::InitForRenderer(content::RenderProcessHost* process) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Bug 103693: SpellCheckHostImpl and SpellCheckProfile should not
  // depend on Profile interface.
  Profile* profile = Profile::FromBrowserContext(process->GetBrowserContext());
  if (SpellCheckFactory::GetHostForProfile(profile) != this)
    return;

  PrefService* prefs = profile->GetPrefs();
  IPC::PlatformFileForTransit file = IPC::InvalidPlatformFileForTransit();

  if (hunspell_dictionary_->GetDictionaryFile() !=
      base::kInvalidPlatformFileValue) {
#if defined(OS_POSIX)
    file = base::FileDescriptor(hunspell_dictionary_->GetDictionaryFile(),
                                false);
#elif defined(OS_WIN)
    BOOL ok = ::DuplicateHandle(::GetCurrentProcess(),
                                hunspell_dictionary_->GetDictionaryFile(),
                                process->GetHandle(),
                                &file,
                                0,
                                false,
                                DUPLICATE_SAME_ACCESS);
    DCHECK(ok) << ::GetLastError();
#endif
  }

  process->Send(new SpellCheckMsg_Init(
      file,
      profile_ ? profile_->GetCustomWords() : CustomWordList(),
      hunspell_dictionary_->GetLanguage(),
      prefs->GetBoolean(prefs::kEnableAutoSpellCorrect)));
}

void SpellCheckHostImpl::AddWord(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (profile_)
    profile_->CustomWordAddedLocally(word);

  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&SpellCheckHostImpl::WriteWordToCustomDictionary,
                 base::Unretained(this), word),
      base::Bind(&SpellCheckHostImpl::AddWordComplete,
                 weak_ptr_factory_.GetWeakPtr(), word));
}

void SpellCheckHostImpl::InformProfileOfInitialization() {
  InformProfileOfInitializationWithCustomWords(NULL);
}

void SpellCheckHostImpl::InformProfileOfInitializationWithCustomWords(
    CustomWordList* custom_words) {
  // Non-null |custom_words| should be given only if the profile is available
  // for simplifying the life-cycle management of the word list.
  DCHECK(profile_ || !custom_words);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (profile_)
    profile_->SpellCheckHostInitialized(custom_words);

  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* process = i.GetCurrentValue();
    if (process)
      InitForRenderer(process);
  }
}

void SpellCheckHostImpl::LoadCustomDictionary(CustomWordList* custom_words) {
  if (!custom_words)
    return;

  // Load custom dictionary for profile.
  if (profile_)
    profile_->LoadCustomDictionary(custom_words);
}

void SpellCheckHostImpl::WriteWordToCustomDictionary(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (profile_)
    profile_->WriteWordToCustomDictionary(word);
}

void SpellCheckHostImpl::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_CREATED);
  content::RenderProcessHost* process =
      content::Source<content::RenderProcessHost>(source).ptr();
  InitForRenderer(process);
}

SpellCheckHostMetrics* SpellCheckHostImpl::GetMetrics() const {
  return metrics_;
}

bool SpellCheckHostImpl::IsReady() const {
  return hunspell_dictionary_->IsReady();
}

bool SpellCheckHostImpl::IsUsingPlatformChecker() const {
  return hunspell_dictionary_->IsUsingPlatformChecker();
}

const base::PlatformFile& SpellCheckHostImpl::GetDictionaryFile() const {
  return hunspell_dictionary_->GetDictionaryFile();
}

const std::string& SpellCheckHostImpl::GetLanguage() const {
  return hunspell_dictionary_->GetLanguage();
}

void SpellCheckHostImpl::AddWordComplete(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new SpellCheckMsg_WordAdded(word));
  }
}
