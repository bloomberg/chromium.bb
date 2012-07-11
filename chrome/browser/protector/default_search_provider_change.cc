// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "chrome/browser/protector/histograms.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_observer.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace protector {

namespace {

// Maximum length of the search engine name to be displayed.
const size_t kMaxDisplayedNameLength = 16;

// Matches TemplateURL with all fields set from the prepopulated data equal
// to fields in another TemplateURL.
class TemplateURLIsSame {
 public:
  // Creates a matcher based on |other|.
  explicit TemplateURLIsSame(const TemplateURL* other) : other_(other) {
  }

  // Returns true if both |other| and |url| are NULL or have same field values.
  bool operator()(const TemplateURL* url) {
    if (url == other_ )
      return true;
    if (!url || !other_)
      return false;
    return url->short_name() == other_->short_name() &&
        url->HasSameKeywordAs(*other_) &&
        url->url() == other_->url() &&
        url->suggestions_url() == other_->suggestions_url() &&
        url->instant_url() == other_->instant_url() &&
        url->safe_for_autoreplace() == other_->safe_for_autoreplace() &&
        url->favicon_url() == other_->favicon_url() &&
        url->show_in_default_list() == other_->show_in_default_list() &&
        url->input_encodings() == other_->input_encodings() &&
        url->prepopulate_id() == other_->prepopulate_id();
  }

 private:
  const TemplateURL* other_;
};

}  // namespace

// Default search engine change tracked by Protector.
class DefaultSearchProviderChange : public BaseSettingChange,
                                    public TemplateURLServiceObserver,
                                    public content::NotificationObserver {
 public:
  DefaultSearchProviderChange(TemplateURL* new_search_provider,
                              TemplateURL* backup_search_provider);

  // BaseSettingChange overrides:
  virtual bool Init(Profile* profile) OVERRIDE;
  virtual void InitWhenDisabled(Profile* profile) OVERRIDE;
  virtual void Apply(Browser* browser) OVERRIDE;
  virtual void Discard(Browser* browser) OVERRIDE;
  virtual void Timeout() OVERRIDE;
  virtual int GetBadgeIconID() const OVERRIDE;
  virtual int GetMenuItemIconID() const OVERRIDE;
  virtual int GetBubbleIconID() const OVERRIDE;
  virtual string16 GetBubbleTitle() const OVERRIDE;
  virtual string16 GetBubbleMessage() const OVERRIDE;
  virtual string16 GetApplyButtonText() const OVERRIDE;
  virtual string16 GetDiscardButtonText() const OVERRIDE;
  virtual DisplayName GetApplyDisplayName() const OVERRIDE;
  virtual GURL GetNewSettingURL() const OVERRIDE;

 private:
  virtual ~DefaultSearchProviderChange();

  // TemplateURLServiceObserver overrides:
  virtual void OnTemplateURLServiceChanged() OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Sets the default search provider to |*search_provider|. If a matching
  // TemplateURL already exists, it is reused and |*search_provider| is reset.
  // Otherwise, adds |*search_provider| to keywords and releases it.
  // In both cases, |*search_provider| is NULL after this call.
  // Returns the new default search provider (either |*search_provider| or the
  // reused TemplateURL).
  const TemplateURL* SetDefaultSearchProvider(
      scoped_ptr<TemplateURL>* search_provider);

  // Returns true if |new_search_provider_| can be used as the default search
  // provider.
  bool NewSearchProviderValid() const;

  // Opens the Search engine settings page in a new tab.
  void OpenSearchEngineSettings(Browser* browser);

  // Returns the TemplateURLService instance for the Profile this change is
  // related to.
  TemplateURLService* GetTemplateURLService();

  // Histogram ID of the new search provider.
  int new_histogram_id_;
  // Indicates that the default search was restored to the prepopulated default
  // search engines.
  bool is_fallback_;
  // Indicates that the the prepopulated default search is the same as
  // |new_search_provider_|.
  bool fallback_is_new_;
  // ID of |new_search_provider_|.
  TemplateURLID new_id_;
  // The default search at the moment the change was detected. Will be used to
  // restore the new default search back if Apply is called. Will be set to
  // |NULL| if removed from the TemplateURLService.
  TemplateURL* new_search_provider_;
  // Default search provider set by Init for the period until user makes a
  // choice and either Apply or Discard is performed. Never is |NULL| during
  // that period since the change will dismiss itself if this provider gets
  // deleted or stops being the default.
  const TemplateURL* default_search_provider_;
  // Stores backup of the default search until it becomes owned by the
  // TemplateURLService or deleted.
  scoped_ptr<TemplateURL> backup_search_provider_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchProviderChange);
};

DefaultSearchProviderChange::DefaultSearchProviderChange(
    TemplateURL* new_search_provider,
    TemplateURL* backup_search_provider)
    : new_histogram_id_(GetSearchProviderHistogramID(new_search_provider)),
      is_fallback_(false),
      fallback_is_new_(false),
      new_search_provider_(new_search_provider),
      default_search_provider_(NULL),
      backup_search_provider_(backup_search_provider) {
  if (backup_search_provider) {
    UMA_HISTOGRAM_ENUMERATION(
        kProtectorHistogramSearchProviderHijacked,
        new_histogram_id_,
        kProtectorMaxSearchProviderID);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        kProtectorHistogramSearchProviderCorrupt,
        new_histogram_id_,
        kProtectorMaxSearchProviderID);
  }
}

DefaultSearchProviderChange::~DefaultSearchProviderChange() {
  if (profile())
    GetTemplateURLService()->RemoveObserver(this);
}

bool DefaultSearchProviderChange::Init(Profile* profile) {
  if (!BaseSettingChange::Init(profile))
    return false;

  if (!backup_search_provider_.get() ||
      !backup_search_provider_->SupportsReplacement()) {
    // Fallback to a prepopulated default search provider, ignoring any
    // overrides in Prefs.
    backup_search_provider_.reset(
        TemplateURLPrepopulateData::GetPrepopulatedDefaultSearch(NULL));
    is_fallback_ = true;
    VLOG(1) << "Fallback search provider: "
            << backup_search_provider_->short_name();
  }

  default_search_provider_ = SetDefaultSearchProvider(&backup_search_provider_);
  DCHECK(default_search_provider_);
  // |backup_search_provider_| should be |NULL| since now.
  DCHECK(!backup_search_provider_.get());
  if (is_fallback_ && default_search_provider_ == new_search_provider_)
    fallback_is_new_ = true;

  int restored_histogram_id =
      GetSearchProviderHistogramID(default_search_provider_);
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramSearchProviderRestored,
      restored_histogram_id,
      kProtectorMaxSearchProviderID);

  if (is_fallback_) {
    VLOG(1) << "Fallback to search provider: "
            << default_search_provider_->short_name();
    UMA_HISTOGRAM_ENUMERATION(
        kProtectorHistogramSearchProviderFallback,
        restored_histogram_id,
        kProtectorMaxSearchProviderID);
  }

  // Listen for the default search provider changes.
  GetTemplateURLService()->AddObserver(this);

  if (new_search_provider_) {
    // Listen for removal of |new_search_provider_|.
    new_id_ = new_search_provider_->id();
    registrar_.Add(
        this, chrome::NOTIFICATION_TEMPLATE_URL_REMOVED,
        content::Source<Profile>(profile->GetOriginalProfile()));
  }

  return true;
}

void DefaultSearchProviderChange::InitWhenDisabled(Profile* profile) {
  // The --no-protector case is handled in TemplateURLService internals.
  // TODO(ivankr): move it here.
  NOTREACHED();
}

void DefaultSearchProviderChange::Apply(Browser* browser) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramSearchProviderApplied,
      new_histogram_id_,
      kProtectorMaxSearchProviderID);

  GetTemplateURLService()->RemoveObserver(this);
  if (NewSearchProviderValid()) {
    GetTemplateURLService()->SetDefaultSearchProvider(new_search_provider_);
  } else {
    // Open settings page in case the new setting is invalid.
    OpenSearchEngineSettings(browser);
  }
}

void DefaultSearchProviderChange::Discard(Browser* browser) {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramSearchProviderDiscarded,
      new_histogram_id_,
      kProtectorMaxSearchProviderID);

  GetTemplateURLService()->RemoveObserver(this);
  if (is_fallback_) {
    // Open settings page in case the old setting is invalid.
    OpenSearchEngineSettings(browser);
  }
  // Nothing to do otherwise since we have already set the search engine
  // to |old_id_| in |Init|.
}

void DefaultSearchProviderChange::Timeout() {
  UMA_HISTOGRAM_ENUMERATION(
      kProtectorHistogramSearchProviderTimeout,
      new_histogram_id_,
      kProtectorMaxSearchProviderID);
}

int DefaultSearchProviderChange::GetBadgeIconID() const {
  return IDR_SEARCH_ENGINE_CHANGE_BADGE;
}

int DefaultSearchProviderChange::GetMenuItemIconID() const {
  return IDR_SEARCH_ENGINE_CHANGE_MENU;
}

int DefaultSearchProviderChange::GetBubbleIconID() const {
  return IDR_SEARCH_ENGINE_CHANGE_ALERT;
}

string16 DefaultSearchProviderChange::GetBubbleTitle() const {
  return l10n_util::GetStringUTF16(IDS_SEARCH_ENGINE_CHANGE_TITLE);
}

string16 DefaultSearchProviderChange::GetBubbleMessage() const {
  if (is_fallback_) {
    return l10n_util::GetStringFUTF16(
        IDS_SEARCH_ENGINE_CHANGE_NO_BACKUP_MESSAGE,
        default_search_provider_->short_name());
  }
  return l10n_util::GetStringUTF16(IDS_SEARCH_ENGINE_CHANGE_MESSAGE);
}

string16 DefaultSearchProviderChange::GetApplyButtonText() const {
  if (NewSearchProviderValid()) {
    // If backup search engine is lost and fallback was made to the current
    // search provider then there is no need to show this button.
    if (fallback_is_new_)
      return string16();
    string16 name = new_search_provider_->short_name();
    return (name.length() > kMaxDisplayedNameLength) ?
        l10n_util::GetStringUTF16(IDS_CHANGE_SEARCH_ENGINE_NO_NAME) :
        l10n_util::GetStringFUTF16(IDS_CHANGE_SEARCH_ENGINE, name);
  }
  if (is_fallback_) {
    // Both settings are lost: don't show this button.
    return string16();
  }
  // New setting is lost, offer to go to settings.
  return l10n_util::GetStringUTF16(IDS_SELECT_SEARCH_ENGINE);
}

string16 DefaultSearchProviderChange::GetDiscardButtonText() const {
  if (!is_fallback_) {
    string16 name = default_search_provider_->short_name();
    return (name.length() > kMaxDisplayedNameLength) ?
        l10n_util::GetStringUTF16(IDS_KEEP_SETTING) :
        l10n_util::GetStringFUTF16(IDS_KEEP_SEARCH_ENGINE, name);
  }
  // Old setting is lost, offer to go to settings.
  return l10n_util::GetStringUTF16(IDS_SELECT_SEARCH_ENGINE);
}

BaseSettingChange::DisplayName
DefaultSearchProviderChange::GetApplyDisplayName() const {
  if (NewSearchProviderValid() &&
      new_search_provider_->short_name().length() <= kMaxDisplayedNameLength) {
    return DisplayName(kDefaultSearchProviderChangeNamePriority,
                       new_search_provider_->short_name());
  }
  // Return the default empty name.
  return BaseSettingChange::GetApplyDisplayName();
}

GURL DefaultSearchProviderChange::GetNewSettingURL() const {
  if (is_fallback_) {
    // Do not collide this change when fallback was made, since the message
    // and Discard behaviour is pretty different.
    return GURL();
  }
  if (!NewSearchProviderValid())
    return GURL();
  return TemplateURLService::GenerateSearchURL(new_search_provider_);
}

void DefaultSearchProviderChange::OnTemplateURLServiceChanged() {
  TemplateURLService* url_service = GetTemplateURLService();
  if (url_service->GetDefaultSearchProvider() != default_search_provider_) {
    VLOG(1) << "Default search provider has been changed by user";
    default_search_provider_ = NULL;
    url_service->RemoveObserver(this);
    // Will delete this DefaultSearchProviderChange instance.
    ProtectorServiceFactory::GetForProfile(profile())->DismissChange(this);
  }
}

void DefaultSearchProviderChange::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_TEMPLATE_URL_REMOVED);
  TemplateURLID id = *content::Details<TemplateURLID>(details).ptr();
  if (id == new_id_)
    new_search_provider_ = NULL;
  registrar_.Remove(this, type, source);
  // TODO(ivankr): should the change be dismissed as well? Probably not,
  // since this may happend due to Sync or whatever. In that case, if user
  // clicks on 'Change to...', the Search engine settings page will be opened.
}

const TemplateURL* DefaultSearchProviderChange::SetDefaultSearchProvider(
    scoped_ptr<TemplateURL>* search_provider) {
  TemplateURLService* url_service = GetTemplateURLService();
  TemplateURLService::TemplateURLVector urls = url_service->GetTemplateURLs();
  TemplateURL* new_default_provider = NULL;

  // Check if this provider already exists and add it otherwise.
  TemplateURLService::TemplateURLVector::const_iterator i =
      std::find_if(urls.begin(), urls.end(),
                   TemplateURLIsSame(search_provider->get()));
  if (i != urls.end()) {
    VLOG(1) << "Provider already exists";
    new_default_provider = *i;
    search_provider->reset();
  } else {
    VLOG(1) << "No match, adding new provider";
    new_default_provider = search_provider->get();
    url_service->Add(search_provider->release());
    UMA_HISTOGRAM_ENUMERATION(
        kProtectorHistogramSearchProviderMissing,
        GetSearchProviderHistogramID(new_default_provider),
        kProtectorMaxSearchProviderID);
  }

  // TODO(ivankr): handle keyword conflicts with existing providers.
  DCHECK(new_default_provider);
  VLOG(1) << "Default search provider set to: "
          << new_default_provider->short_name();
  url_service->SetDefaultSearchProvider(new_default_provider);
  return new_default_provider;
}

bool DefaultSearchProviderChange::NewSearchProviderValid() const {
  return new_search_provider_ && new_search_provider_->SupportsReplacement();
}

void DefaultSearchProviderChange::OpenSearchEngineSettings(Browser* browser) {
  ProtectorServiceFactory::GetForProfile(profile())->OpenTab(
      GURL(std::string(chrome::kChromeUISettingsURL) +
           chrome::kSearchEnginesSubPage), browser);
}

TemplateURLService* DefaultSearchProviderChange::GetTemplateURLService() {
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  DCHECK(url_service);
  return url_service;
}

BaseSettingChange* CreateDefaultSearchProviderChange(TemplateURL* actual,
                                                     TemplateURL* backup) {
  return new DefaultSearchProviderChange(actual, backup);
}

}  // namespace protector
