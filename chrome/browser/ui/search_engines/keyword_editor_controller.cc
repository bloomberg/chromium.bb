// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"

using content::UserMetricsAction;

KeywordEditorController::KeywordEditorController(Profile* profile)
    : profile_(profile) {
  table_model_.reset(new TemplateURLTableModel(
      TemplateURLServiceFactory::GetForProfile(profile)));
}

KeywordEditorController::~KeywordEditorController() {
}

// static
// TODO(rsesek): Other platforms besides Mac should remember window
// placement. http://crbug.com/22269
void KeywordEditorController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kKeywordEditorWindowPlacement);
}

int KeywordEditorController::AddTemplateURL(const string16& title,
                                            const string16& keyword,
                                            const std::string& url) {
  DCHECK(!url.empty());

  content::RecordAction(UserMetricsAction("KeywordEditor_AddKeyword"));

  const int new_index = table_model_->last_other_engine_index();
  table_model_->Add(new_index, title, keyword, url);

  return new_index;
}

void KeywordEditorController::ModifyTemplateURL(TemplateURL* template_url,
                                                const string16& title,
                                                const string16& keyword,
                                                const std::string& url) {
  DCHECK(!url.empty());
  const int index = table_model_->IndexOfTemplateURL(template_url);
  if (index == -1) {
    // Will happen if url was deleted out from under us while the user was
    // editing it.
    return;
  }

  // Don't do anything if the entry didn't change.
  if ((template_url->short_name() == title) &&
      (template_url->keyword() == keyword) && (template_url->url() == url))
    return;

  table_model_->ModifyTemplateURL(index, title, keyword, url);

  content::RecordAction(UserMetricsAction("KeywordEditor_ModifiedKeyword"));
}

bool KeywordEditorController::CanEdit(const TemplateURL* url) const {
  return !url_model()->is_default_search_managed() ||
      url != url_model()->GetDefaultSearchProvider();
}

bool KeywordEditorController::CanMakeDefault(const TemplateURL* url) const {
  return url_model()->CanMakeDefault(url);
}

bool KeywordEditorController::CanRemove(const TemplateURL* url) const {
  return url != url_model()->GetDefaultSearchProvider();
}

void KeywordEditorController::RemoveTemplateURL(int index) {
  table_model_->Remove(index);
  content::RecordAction(UserMetricsAction("KeywordEditor_RemoveKeyword"));
}

int KeywordEditorController::MakeDefaultTemplateURL(int index) {
  return table_model_->MakeDefaultTemplateURL(index);
}

bool KeywordEditorController::loaded() const {
  return url_model()->loaded();
}

TemplateURL* KeywordEditorController::GetTemplateURL(int index) {
  return table_model_->GetTemplateURL(index);
}

TemplateURLService* KeywordEditorController::url_model() const {
  return table_model_->template_url_service();
}
