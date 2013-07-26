// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/template_url_table_model.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/favicon/favicon_types.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image_skia.h"

// Group IDs used by TemplateURLTableModel.
static const int kMainGroupID = 0;
static const int kOtherGroupID = 1;
static const int kExtensionGroupID = 2;

// ModelEntry ----------------------------------------------------

// ModelEntry wraps a TemplateURL as returned from the TemplateURL.
// ModelEntry also tracks state information about the URL.

// Icon used while loading, or if a specific favicon can't be found.
static gfx::ImageSkia* default_icon = NULL;

class ModelEntry {
 public:
  ModelEntry(TemplateURLTableModel* model, TemplateURL* template_url)
      : template_url_(template_url),
        load_state_(NOT_LOADED),
        model_(model) {
    if (!default_icon) {
      default_icon = ResourceBundle::GetSharedInstance().
          GetImageSkiaNamed(IDR_DEFAULT_FAVICON);
    }
  }

  TemplateURL* template_url() {
    return template_url_;
  }

  gfx::ImageSkia GetIcon() {
    if (load_state_ == NOT_LOADED)
      LoadFavicon();
    if (!favicon_.isNull())
      return favicon_;
    return *default_icon;
  }

  // Resets internal status so that the next time the icon is asked for its
  // fetched again. This should be invoked if the url is modified.
  void ResetIcon() {
    load_state_ = NOT_LOADED;
    favicon_ = gfx::ImageSkia();
  }

 private:
  // State of the favicon.
  enum LoadState {
    NOT_LOADED,
    LOADING,
    LOADED
  };

  void LoadFavicon() {
    load_state_ = LOADED;
    FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
        model_->template_url_service()->profile(), Profile::EXPLICIT_ACCESS);
    if (!favicon_service)
      return;
    GURL favicon_url = template_url()->favicon_url();
    if (!favicon_url.is_valid()) {
      // The favicon url isn't always set. Guess at one here.
      if (template_url_->url_ref().IsValid()) {
        GURL url(template_url_->url());
        if (url.is_valid())
          favicon_url = TemplateURL::GenerateFaviconURL(url);
      }
      if (!favicon_url.is_valid())
        return;
    }
    load_state_ = LOADING;
    favicon_service->GetFaviconImage(
        favicon_url, chrome::FAVICON, gfx::kFaviconSize,
        base::Bind(&ModelEntry::OnFaviconDataAvailable,
                   base::Unretained(this)),
        &tracker_);
  }

  void OnFaviconDataAvailable(const chrome::FaviconImageResult& image_result) {
    load_state_ = LOADED;
    if (!image_result.image.IsEmpty()) {
      favicon_ = image_result.image.AsImageSkia();
      model_->FaviconAvailable(this);
    }
  }

  TemplateURL* template_url_;
  gfx::ImageSkia favicon_;
  LoadState load_state_;
  TemplateURLTableModel* model_;
  CancelableTaskTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(ModelEntry);
};

// TemplateURLTableModel -----------------------------------------

TemplateURLTableModel::TemplateURLTableModel(
    TemplateURLService* template_url_service)
    : observer_(NULL),
      template_url_service_(template_url_service) {
  DCHECK(template_url_service);
  template_url_service_->Load();
  template_url_service_->AddObserver(this);
  Reload();
}

TemplateURLTableModel::~TemplateURLTableModel() {
  template_url_service_->RemoveObserver(this);
  STLDeleteElements(&entries_);
  entries_.clear();
}

void TemplateURLTableModel::Reload() {
  STLDeleteElements(&entries_);
  entries_.clear();

  TemplateURLService::TemplateURLVector urls =
      template_url_service_->GetTemplateURLs();

  std::vector<ModelEntry*> default_entries, other_entries, extension_entries;
  // Keywords that can be made the default first.
  for (TemplateURLService::TemplateURLVector::iterator i = urls.begin();
       i != urls.end(); ++i) {
    TemplateURL* template_url = *i;
    // NOTE: we don't use ShowInDefaultList here to avoid items bouncing around
    // the lists while editing.
    if (template_url->show_in_default_list())
      default_entries.push_back(new ModelEntry(this, template_url));
    else if (template_url->IsExtensionKeyword())
      extension_entries.push_back(new ModelEntry(this, template_url));
    else
      other_entries.push_back(new ModelEntry(this, template_url));
  }

  last_search_engine_index_ = static_cast<int>(default_entries.size());
  last_other_engine_index_ = last_search_engine_index_ +
      static_cast<int>(other_entries.size());

  entries_.insert(entries_.end(),
                  default_entries.begin(),
                  default_entries.end());

  entries_.insert(entries_.end(),
                  other_entries.begin(),
                  other_entries.end());

  entries_.insert(entries_.end(),
                  extension_entries.begin(),
                  extension_entries.end());

  if (observer_)
    observer_->OnModelChanged();
}

int TemplateURLTableModel::RowCount() {
  return static_cast<int>(entries_.size());
}

string16 TemplateURLTableModel::GetText(int row, int col_id) {
  DCHECK(row >= 0 && row < RowCount());
  const TemplateURL* url = entries_[row]->template_url();
  if (col_id == IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN) {
    string16 url_short_name = url->short_name();
    // TODO(xji): Consider adding a special case if the short name is a URL,
    // since those should always be displayed LTR. Please refer to
    // http://crbug.com/6726 for more information.
    base::i18n::AdjustStringForLocaleDirection(&url_short_name);
    return (template_url_service_->GetDefaultSearchProvider() == url) ?
        l10n_util::GetStringFUTF16(IDS_SEARCH_ENGINES_EDITOR_DEFAULT_ENGINE,
                                   url_short_name) : url_short_name;
  }

  DCHECK_EQ(IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN, col_id);
  // Keyword should be domain name. Force it to have LTR directionality.
  return base::i18n::GetDisplayStringInLTRDirectionality(url->keyword());
}

gfx::ImageSkia TemplateURLTableModel::GetIcon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  return entries_[row]->GetIcon();
}

void TemplateURLTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

bool TemplateURLTableModel::HasGroups() {
  return true;
}

TemplateURLTableModel::Groups TemplateURLTableModel::GetGroups() {
  Groups groups;

  Group search_engine_group;
  search_engine_group.title =
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_MAIN_SEPARATOR);
  search_engine_group.id = kMainGroupID;
  groups.push_back(search_engine_group);

  Group other_group;
  other_group.title =
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_OTHER_SEPARATOR);
  other_group.id = kOtherGroupID;
  groups.push_back(other_group);

  Group extension_group;
  extension_group.title =
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_EXTENSIONS_SEPARATOR);
  extension_group.id = kExtensionGroupID;
  groups.push_back(extension_group);

  return groups;
}

int TemplateURLTableModel::GetGroupID(int row) {
  DCHECK(row >= 0 && row < RowCount());
  if (row < last_search_engine_index_)
    return kMainGroupID;
  return row < last_other_engine_index_ ? kOtherGroupID : kExtensionGroupID;
}

void TemplateURLTableModel::Remove(int index) {
  // Remove the observer while we modify the model, that way we don't need to
  // worry about the model calling us back when we mutate it.
  template_url_service_->RemoveObserver(this);
  TemplateURL* template_url = GetTemplateURL(index);

  scoped_ptr<ModelEntry> entry(RemoveEntry(index));

  // Make sure to remove from the table model first, otherwise the
  // TemplateURL would be freed.
  template_url_service_->Remove(template_url);
  template_url_service_->AddObserver(this);
}

void TemplateURLTableModel::Add(int index,
                                const string16& short_name,
                                const string16& keyword,
                                const std::string& url) {
  DCHECK(index >= 0 && index <= RowCount());
  DCHECK(!url.empty());
  template_url_service_->RemoveObserver(this);
  TemplateURLData data;
  data.short_name = short_name;
  data.SetKeyword(keyword);
  data.SetURL(url);
  TemplateURL* turl = new TemplateURL(template_url_service_->profile(), data);
  template_url_service_->Add(turl);
  scoped_ptr<ModelEntry> entry(new ModelEntry(this, turl));
  template_url_service_->AddObserver(this);
  AddEntry(index, entry.Pass());
}

void TemplateURLTableModel::ModifyTemplateURL(int index,
                                              const string16& title,
                                              const string16& keyword,
                                              const std::string& url) {
  DCHECK(index >= 0 && index <= RowCount());
  DCHECK(!url.empty());
  TemplateURL* template_url = GetTemplateURL(index);
  // The default search provider should support replacement.
  DCHECK(template_url_service_->GetDefaultSearchProvider() != template_url ||
         template_url->SupportsReplacement());
  template_url_service_->RemoveObserver(this);
  template_url_service_->ResetTemplateURL(template_url, title, keyword, url);
  template_url_service_->AddObserver(this);
  ReloadIcon(index);  // Also calls NotifyChanged().
}

void TemplateURLTableModel::ReloadIcon(int index) {
  DCHECK(index >= 0 && index < RowCount());

  entries_[index]->ResetIcon();

  NotifyChanged(index);
}

TemplateURL* TemplateURLTableModel::GetTemplateURL(int index) {
  return entries_[index]->template_url();
}

int TemplateURLTableModel::IndexOfTemplateURL(
    const TemplateURL* template_url) {
  for (std::vector<ModelEntry*>::iterator i = entries_.begin();
       i != entries_.end(); ++i) {
    ModelEntry* entry = *i;
    if (entry->template_url() == template_url)
      return static_cast<int>(i - entries_.begin());
  }
  return -1;
}

int TemplateURLTableModel::MoveToMainGroup(int index) {
  if (index < last_search_engine_index_)
    return index;  // Already in the main group.

  scoped_ptr<ModelEntry> current_entry(RemoveEntry(index));
  const int new_index = last_search_engine_index_++;
  AddEntry(new_index, current_entry.Pass());
  return new_index;
}

int TemplateURLTableModel::MakeDefaultTemplateURL(int index) {
  if (index < 0 || index >= RowCount()) {
    NOTREACHED();
    return -1;
  }

  TemplateURL* keyword = GetTemplateURL(index);
  const TemplateURL* current_default =
      template_url_service_->GetDefaultSearchProvider();
  if (current_default == keyword)
    return -1;

  template_url_service_->RemoveObserver(this);
  template_url_service_->SetDefaultSearchProvider(keyword);
  template_url_service_->AddObserver(this);

  // The formatting of the default engine is different; notify the table that
  // both old and new entries have changed.
  if (current_default != NULL) {
    int old_index = IndexOfTemplateURL(current_default);
    // current_default may not be in the list of TemplateURLs if the database is
    // corrupt and the default TemplateURL is used from preferences
    if (old_index >= 0)
      NotifyChanged(old_index);
  }
  const int new_index = IndexOfTemplateURL(keyword);
  NotifyChanged(new_index);

  // Make sure the new default is in the main group.
  return MoveToMainGroup(index);
}

void TemplateURLTableModel::NotifyChanged(int index) {
  if (observer_) {
    DCHECK_GE(index, 0);
    observer_->OnItemsChanged(index, 1);
  }
}

void TemplateURLTableModel::FaviconAvailable(ModelEntry* entry) {
  std::vector<ModelEntry*>::iterator i =
      std::find(entries_.begin(), entries_.end(), entry);
  DCHECK(i != entries_.end());
  NotifyChanged(static_cast<int>(i - entries_.begin()));
}

void TemplateURLTableModel::OnTemplateURLServiceChanged() {
  Reload();
}

scoped_ptr<ModelEntry> TemplateURLTableModel::RemoveEntry(int index) {
  scoped_ptr<ModelEntry> entry(entries_[index]);
  entries_.erase(index + entries_.begin());
  if (index < last_search_engine_index_)
    --last_search_engine_index_;
  if (index < last_other_engine_index_)
    --last_other_engine_index_;
  if (observer_)
    observer_->OnItemsRemoved(index, 1);
  return entry.Pass();
}

void TemplateURLTableModel::AddEntry(int index, scoped_ptr<ModelEntry> entry) {
  entries_.insert(entries_.begin() + index, entry.release());
  if (index <= last_other_engine_index_)
    ++last_other_engine_index_;
  if (observer_)
    observer_->OnItemsAdded(index, 1);
}
