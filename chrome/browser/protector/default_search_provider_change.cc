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
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "googleurl/src/gurl.h"
#include "ui/base/l10n/l10n_util.h"

namespace protector {

namespace {

// Maximum length of the search engine name to be displayed.
const size_t kMaxDisplayedNameLength = 10;

}  // namespace

class DefaultSearchProviderChange : public SettingChange {
 public:
  DefaultSearchProviderChange(const TemplateURL* old_url,
                              const TemplateURL* new_url);

  // SettingChange overrides:
  virtual bool Init(Protector* protector) OVERRIDE;
  virtual void Apply(Protector* protector) OVERRIDE;
  virtual void Discard(Protector* protector) OVERRIDE;
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetMessage() const OVERRIDE;
  virtual string16 GetApplyButtonText() const OVERRIDE;
  virtual string16 GetDiscardButtonText() const OVERRIDE;

 private:
  virtual ~DefaultSearchProviderChange();

  // Sets the given default search provider to profile that |protector| is
  // guarding. Returns the |TemplateURL| instance the default search provider
  // has been set to. If no search provider with |id| exists and
  // |allow_fallback| is true, sets one of the prepoluated search providers.
  const TemplateURL* SetDefaultSearchProvider(Protector* protector,
                                              int64 id,
                                              bool allow_fallback);

  // Opens the Search engine settings page in a new tab.
  void OpenSearchEngineSettings(Protector* protector);

  int64 old_id_;
  int64 new_id_;
  // ID of the search engine that we fall back to if the backup is lost.
  int64 fallback_id_;
  string16 old_name_;
  string16 new_name_;
  // Name of the search engine that we fall back to if the backup is lost.
  string16 fallback_name_;
  string16 product_name_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchProviderChange);
};

DefaultSearchProviderChange::DefaultSearchProviderChange(
    const TemplateURL* old_url,
    const TemplateURL* new_url)
    : old_id_(0),
      new_id_(0),
      fallback_id_(0),
      product_name_(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)) {
  if (new_url) {
    new_id_ = new_url->id();
    new_name_ = new_url->short_name();
  }
  if (old_url) {
    old_id_ = old_url->id();
    old_name_ = old_url->short_name();
  }
}

DefaultSearchProviderChange::~DefaultSearchProviderChange() {
}

bool DefaultSearchProviderChange::Init(Protector* protector) {
  // Initially reset the search engine to its previous setting.
  const TemplateURL* current_url =
      SetDefaultSearchProvider(protector, old_id_, true);
  if (!current_url)
    return false;
  if (!old_id_ || current_url->id() != old_id_) {
    // Old settings is lost or invalid, so we had to fall back to one of the
    // prepopulated search engines.
    fallback_id_ = current_url->id();
    fallback_name_ = current_url->short_name();
    VLOG(1) << "Fallback to " << fallback_name_;
  }
  return true;
}

void DefaultSearchProviderChange::Apply(Protector* protector) {
  // TODO(avayvod): Add histrogram.
  if (!new_id_) {
    // Open settings page in case the new setting is invalid.
    OpenSearchEngineSettings(protector);
  } else {
    SetDefaultSearchProvider(protector, new_id_, false);
  }
}

void DefaultSearchProviderChange::Discard(Protector* protector) {
  // TODO(avayvod): Add histrogram.
  if (!old_id_) {
    // Open settings page in case the old setting is invalid.
    OpenSearchEngineSettings(protector);
  }
  // Nothing to do otherwise since we have already set the search engine
  // to |old_id_| in |Init|.
}

string16 DefaultSearchProviderChange::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_SEARCH_ENGINE_CHANGE_TITLE);
}

string16 DefaultSearchProviderChange::GetMessage() const {
  if (fallback_name_.empty())
    return l10n_util::GetStringFUTF16(
        IDS_SEARCH_ENGINE_CHANGE_MESSAGE, product_name_);
  else
    return l10n_util::GetStringFUTF16(
        IDS_SEARCH_ENGINE_CHANGE_NO_BACKUP_MESSAGE,
        product_name_, fallback_name_);
}

string16 DefaultSearchProviderChange::GetApplyButtonText() const {
  if (new_id_) {
    if (new_id_ == fallback_id_) {
      // Old search engine is lost, the fallback search engine is the same as
      // the new one so no need to show this button.
      return string16();
    }
    if (new_name_.length() > kMaxDisplayedNameLength)
      return l10n_util::GetStringUTF16(IDS_CHANGE_SEARCH_ENGINE_NO_NAME);
    else
      return l10n_util::GetStringFUTF16(IDS_CHANGE_SEARCH_ENGINE, new_name_);
  } else if (old_id_) {
    // New setting is lost, offer to go to settings.
    return l10n_util::GetStringUTF16(IDS_SELECT_SEARCH_ENGINE);
  } else {
    // Both settings are lost: don't show this button.
    return string16();
  }
}

string16 DefaultSearchProviderChange::GetDiscardButtonText() const {
  if (old_id_) {
    if (new_name_.length() > kMaxDisplayedNameLength)
      return l10n_util::GetStringUTF16(IDS_KEEP_SETTING);
    else
      return l10n_util::GetStringFUTF16(IDS_KEEP_SEARCH_ENGINE, old_name_);
  } else {
    // Old setting is lost, offer to go to settings.
    return l10n_util::GetStringUTF16(IDS_SELECT_SEARCH_ENGINE);
  }
}

const TemplateURL* DefaultSearchProviderChange::SetDefaultSearchProvider(
    Protector* protector,
    int64 id,
    bool allow_fallback) {
  TemplateURLService* url_service = protector->GetTemplateURLService();
  if (!url_service) {
    NOTREACHED() << "Can't get TemplateURLService object.";
    return NULL;
  }
  const TemplateURL* url = NULL;
  if (id) {
    const TemplateURLService::TemplateURLVector& urls =
        url_service->GetTemplateURLs();
    for (size_t i = 0; i < urls.size(); ++i) {
      if (urls[i]->id() == id) {
        url = urls[i];
        break;
      }
    }
  }
  if (!url && allow_fallback) {
    url = url_service->FindNewDefaultSearchProvider();
    DCHECK(url);
  }
  if (url) {
    url_service->SetDefaultSearchProvider(url);
    VLOG(1) << "Default search provider set to: " << url->short_name();
  }
  return url;
}

void DefaultSearchProviderChange::OpenSearchEngineSettings(
    Protector* protector) {
  protector->OpenTab(
      GURL(std::string(chrome::kChromeUISettingsURL) +
           chrome::kSearchEnginesSubPage));
}

SettingChange* CreateDefaultSearchProviderChange(
    const TemplateURL* actual,
    const TemplateURL* backup) {
  return new DefaultSearchProviderChange(backup, actual);
}

}  // namespace protector
