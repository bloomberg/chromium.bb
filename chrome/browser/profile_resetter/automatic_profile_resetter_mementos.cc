// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/automatic_profile_resetter_mementos.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

using base::DictionaryValue;


// AutomaticProfileResetter::PreferenceHostedPromptMemento -------------------

PreferenceHostedPromptMemento::PreferenceHostedPromptMemento(Profile* profile)
    : profile_(profile) {}

PreferenceHostedPromptMemento::~PreferenceHostedPromptMemento() {}

std::string PreferenceHostedPromptMemento::ReadValue() const {
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  return prefs->GetString(prefs::kProfileResetPromptMementoInProfilePrefs);
}

void PreferenceHostedPromptMemento::StoreValue(const std::string& value) {
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  prefs->SetString(prefs::kProfileResetPromptMementoInProfilePrefs, value);
}


// AutomaticProfileResetter::LocalStateHostedPromptMemento -------------------

LocalStateHostedPromptMemento::LocalStateHostedPromptMemento(Profile* profile)
    : profile_(profile) {}

LocalStateHostedPromptMemento::~LocalStateHostedPromptMemento() {}

std::string LocalStateHostedPromptMemento::ReadValue() const {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  const base::DictionaryValue* prompt_shown_dict = local_state->GetDictionary(
      prefs::kProfileResetPromptMementosInLocalState);
  std::string profile_key = GetProfileKey();
  if (!prompt_shown_dict || profile_key.empty()) {
    NOTREACHED();
    return std::string();
  }
  std::string value;
  return prompt_shown_dict->GetStringWithoutPathExpansion(profile_key, &value) ?
      value : std::string();
}

void LocalStateHostedPromptMemento::StoreValue(const std::string& value) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  DictionaryPrefUpdate prompt_shown_dict_update(
      local_state, prefs::kProfileResetPromptMementosInLocalState);
  std::string profile_key = GetProfileKey();
  if (profile_key.empty()) {
    NOTREACHED();
    return;
  }
  prompt_shown_dict_update.Get()->SetStringWithoutPathExpansion(profile_key,
                                                                value);
}

std::string LocalStateHostedPromptMemento::GetProfileKey() const {
  return profile_->GetPath().BaseName().MaybeAsASCII();
}


// AutomaticProfileResetter::FileHostedPromptMemento -------------------------

FileHostedPromptMemento::FileHostedPromptMemento(Profile* profile)
    : profile_(profile) {}

FileHostedPromptMemento::~FileHostedPromptMemento() {}

void FileHostedPromptMemento::ReadValue(
    const ReadValueCallback& callback) const {
  base::FilePath path = GetMementoFilePath();
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ReadValueOnFileThread, path),
      callback);
}

void FileHostedPromptMemento::StoreValue(const std::string& value) {
  base::FilePath path = GetMementoFilePath();
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&StoreValueOnFileThread, path, value));
}

std::string FileHostedPromptMemento::ReadValueOnFileThread(
    const base::FilePath& memento_file_path) {
  std::string value;
  base::ReadFileToString(memento_file_path, &value);
  return value;
}

void FileHostedPromptMemento::StoreValueOnFileThread(
    const base::FilePath& memento_file_path,
    const std::string& value) {
  int retval =
      base::WriteFile(memento_file_path, value.c_str(), value.size());
  DCHECK_EQ(retval, (int)value.size());
}

base::FilePath FileHostedPromptMemento::GetMementoFilePath() const {
  return profile_->GetPath().Append(chrome::kResetPromptMementoFilename);
}
