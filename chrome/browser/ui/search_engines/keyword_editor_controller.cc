// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"

#include "base/prefs/pref_registry_simple.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/common/pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/user_metrics.h"

using base::UserMetricsAction;

KeywordEditorController::KeywordEditorController(Profile* profile)
    : url_model_(TemplateURLServiceFactory::GetForProfile(profile)) {
  table_model_.reset(new TemplateURLTableModel(
      url_model_,
      FaviconServiceFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS)));
}

KeywordEditorController::~KeywordEditorController() {
}

// static
// TODO(rsesek): Other platforms besides Mac should remember window
// placement. http://crbug.com/22269
void KeywordEditorController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kKeywordEditorWindowPlacement);
}

int KeywordEditorController::AddTemplateURL(const base::string16& title,
                                            const base::string16& keyword,
                                            const std::string& url) {
  DCHECK(!url.empty());

  content::RecordAction(UserMetricsAction("KeywordEditor_AddKeyword"));

  const int new_index = table_model_->last_other_engine_index();
  table_model_->Add(new_index, title, keyword, url);

  return new_index;
}

void KeywordEditorController::ModifyTemplateURL(TemplateURL* template_url,
                                                const base::string16& title,
                                                const base::string16& keyword,
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
  return (url->GetType() != TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION) &&
      (!url_model_->is_default_search_managed() ||
       (url != url_model_->GetDefaultSearchProvider()));
}

bool KeywordEditorController::CanMakeDefault(const TemplateURL* url) const {
  return url_model_->CanMakeDefault(url);
}

bool KeywordEditorController::CanRemove(const TemplateURL* url) const {
  return url != url_model_->GetDefaultSearchProvider();
}

void KeywordEditorController::RemoveTemplateURL(int index) {
  table_model_->Remove(index);
  content::RecordAction(UserMetricsAction("KeywordEditor_RemoveKeyword"));
}

TemplateURL* KeywordEditorController::GetDefaultSearchProvider() {
  return url_model_->GetDefaultSearchProvider();
}

int KeywordEditorController::MakeDefaultTemplateURL(int index) {
  return table_model_->MakeDefaultTemplateURL(index);
}

bool KeywordEditorController::loaded() const {
  return url_model_->loaded();
}

TemplateURL* KeywordEditorController::GetTemplateURL(int index) {
  return table_model_->GetTemplateURL(index);
}
