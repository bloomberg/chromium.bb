// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/protector/protector.h"
#include "chrome/browser/protector/setting_change.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

namespace protector {

class DefaultSearchProviderChange : public SettingChange {
 public:
  DefaultSearchProviderChange(const TemplateURL* old_url,
                              const TemplateURL* new_url);

  // SettingChange overrides:
  virtual string16 GetOldSetting() const OVERRIDE;
  virtual string16 GetNewSetting() const OVERRIDE;
  virtual void Accept(Protector* protector) OVERRIDE;
  virtual void Revert(Protector* protector) OVERRIDE;
  virtual void DoDefault(Protector* protector) OVERRIDE;

 private:
  virtual ~DefaultSearchProviderChange();

  // Sets the given default search provider to profile that |protector| is
  // guarding.
  void SetDefaultSearchProvider(Protector* protector, int64 id);

  int64 old_id_;
  int64 new_id_;
  string16 old_name_;
  string16 new_name_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchProviderChange);
};

DefaultSearchProviderChange::DefaultSearchProviderChange(
    const TemplateURL* old_url,
    const TemplateURL* new_url)
    : SettingChange(kSearchEngineChanged),
      old_id_(0),
      new_id_(0) {
  DCHECK(new_url);
  new_id_ = new_url->id();
  new_name_ = new_url->short_name();
  if (old_url) {
    old_id_ = old_url->id();
    old_name_ = old_url->short_name();
  }
}

DefaultSearchProviderChange::~DefaultSearchProviderChange() {
}

string16 DefaultSearchProviderChange::GetOldSetting() const {
  return old_name_;
}

string16 DefaultSearchProviderChange::GetNewSetting() const {
  return new_name_;
}

void DefaultSearchProviderChange::Accept(Protector* protector) {
  SetDefaultSearchProvider(protector, new_id_);
  // TODO(avayvod): Add histrogram.
}

void DefaultSearchProviderChange::Revert(Protector* protector) {
  SetDefaultSearchProvider(protector, old_id_);
  if (!old_id_) {
    // Open settings page in case the original setting was lost.
    protector->OpenTab(
        GURL(std::string(chrome::kChromeUISettingsURL) +
            chrome::kSearchEnginesSubPage));
  }
  // TODO(avayvod): Add histrogram.
}

void DefaultSearchProviderChange::DoDefault(Protector* protector) {
  SetDefaultSearchProvider(protector, old_id_);
  // TODO(avayvod): Add histrogram.
}

void DefaultSearchProviderChange::SetDefaultSearchProvider(
    Protector* protector,
    int64 id) {
  DCHECK(protector);
  TemplateURLService* url_service = protector->GetTemplateURLService();
  if (!url_service) {
    LOG(WARNING) << "Can't get TemplateURLService object.";
    return;
  }
  const TemplateURL* url = NULL;
  const TemplateURLService::TemplateURLVector& urls =
      url_service->GetTemplateURLs();
  for (size_t i = 0; i < urls.size(); ++i)
    if (urls[i]->id() == id) {
      url = urls[i];
      break;
    }
  if (!url)
    url = url_service->FindNewDefaultSearchProvider();
  url_service->SetDefaultSearchProvider(url);
}

SettingChange* CreateDefaultSearchProviderChange(
    const TemplateURL* actual,
    const TemplateURL* backup) {
  return new DefaultSearchProviderChange(backup, actual);
}

}  // namespace protector
