// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_application_list_model.h"

#include <algorithm>
#include <set>

#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "ui/base/l10n/l10n_util_collator.h"

class ExtensionNameComparator {
 public:
  explicit ExtensionNameComparator(icu::Collator* collator);
  bool operator()(const Extension* x, const Extension* y);

 private:
  icu::Collator* collator_;
};

ExtensionNameComparator::ExtensionNameComparator(icu::Collator* collator)
  : collator_(collator) {
}

bool ExtensionNameComparator::operator()(const Extension* x,
                                         const Extension* y) {
  return l10n_util::StringComparator<string16>(collator_)(
    UTF8ToUTF16(x->name()),
    UTF8ToUTF16(y->name()));
}

// Background application representation, private to the
// BackgroundApplicationListModel class.
class BackgroundApplicationListModel::Application
  : public ImageLoadingTracker::Observer {
 public:
  Application(BackgroundApplicationListModel* model,
              const Extension* an_extension);

  virtual ~Application();

  // Invoked when a request icon is available.
  virtual void OnImageLoaded(SkBitmap* image,
                             const ExtensionResource& resource,
                             int index);

  // Uses the FILE thread to request this extension's icon, sized
  // appropriately.
  void RequestIcon(Extension::Icons size);

  const Extension* extension_;
  scoped_ptr<SkBitmap> icon_;
  BackgroundApplicationListModel* model_;
  ImageLoadingTracker tracker_;
};

namespace {
void GetServiceApplications(ExtensionService* service,
                            ExtensionList* applications_result) {
  const ExtensionList* extensions = service->extensions();

  for (ExtensionList::const_iterator cursor = extensions->begin();
       cursor != extensions->end();
       ++cursor) {
    const Extension* extension = *cursor;
    if (BackgroundApplicationListModel::IsBackgroundApp(*extension))
      applications_result->push_back(extension);
  }
  std::string locale = g_browser_process->GetApplicationLocale();
  icu::Locale loc(locale.c_str());
  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(loc, error));
  sort(applications_result->begin(), applications_result->end(),
       ExtensionNameComparator(collator.get()));
}

bool HasBackgroundAppPermission(
    const std::set<std::string>& api_permissions) {
  return Extension::HasApiPermission(
      api_permissions, Extension::kBackgroundPermission);
}
}  // namespace

void
BackgroundApplicationListModel::Observer::OnApplicationDataChanged(
    const Extension* extension) {
}

void
BackgroundApplicationListModel::Observer::OnApplicationListChanged() {
}

BackgroundApplicationListModel::Observer::~Observer() {
}

BackgroundApplicationListModel::Application::~Application() {
}

BackgroundApplicationListModel::Application::Application(
    BackgroundApplicationListModel* model,
    const Extension* extension)
    : extension_(extension),
      icon_(NULL),
      model_(model),
      ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)) {
}

void BackgroundApplicationListModel::Application::OnImageLoaded(
    SkBitmap* image,
    const ExtensionResource& resource,
    int index) {
  if (!image)
    return;
  icon_.reset(new SkBitmap(*image));
  model_->OnApplicationDataChanged(extension_);
}

void BackgroundApplicationListModel::Application::RequestIcon(
    Extension::Icons size) {
  ExtensionResource resource = extension_->GetIconResource(
      size, ExtensionIconSet::MATCH_BIGGER);
  tracker_.LoadImage(extension_, resource, gfx::Size(size, size),
                     ImageLoadingTracker::CACHE);
}

BackgroundApplicationListModel::~BackgroundApplicationListModel() {
  STLDeleteContainerPairSecondPointers(applications_.begin(),
                                       applications_.end());
}

BackgroundApplicationListModel::BackgroundApplicationListModel(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  registrar_.Add(this,
                 NotificationType::EXTENSION_LOADED,
                 Source<Profile>(profile));
  registrar_.Add(this,
                 NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(profile));
  registrar_.Add(this,
                 NotificationType::EXTENSIONS_READY,
                 Source<Profile>(profile));
  ExtensionService* service = profile->GetExtensionService();
  if (service && service->is_ready())
    Update();
}

void BackgroundApplicationListModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BackgroundApplicationListModel::AssociateApplicationData(
    const Extension* extension) {
  DCHECK(IsBackgroundApp(*extension));
  Application* application = FindApplication(extension);
  if (!application) {
    // App position is used as a dynamic command and so must be less than any
    // predefined command id.
    if (applications_.size() >= IDC_MinimumLabelValue) {
      LOG(ERROR) << "Background application limit of " << IDC_MinimumLabelValue
                 << " exceeded.  Ignoring.";
      return;
    }
    application = new Application(this, extension);
    applications_[extension->id()] = application;
    application->RequestIcon(Extension::EXTENSION_ICON_BITTY);
  }
}

void BackgroundApplicationListModel::DissociateApplicationData(
    const Extension* extension) {
  ApplicationMap::iterator found = applications_.find(extension->id());
  if (found != applications_.end()) {
    delete found->second;
    applications_.erase(found);
  }
}

const Extension* BackgroundApplicationListModel::GetExtension(
    int position) const {
  DCHECK(position >= 0 && static_cast<size_t>(position) < extensions_.size());
  return extensions_[position];
}

const BackgroundApplicationListModel::Application*
BackgroundApplicationListModel::FindApplication(
    const Extension* extension) const {
  const std::string& id = extension->id();
  ApplicationMap::const_iterator found = applications_.find(id);
  return (found == applications_.end()) ? NULL : found->second;
}

BackgroundApplicationListModel::Application*
BackgroundApplicationListModel::FindApplication(const Extension* extension) {
  const std::string& id = extension->id();
  ApplicationMap::iterator found = applications_.find(id);
  return (found == applications_.end()) ? NULL : found->second;
}

const SkBitmap* BackgroundApplicationListModel::GetIcon(
    const Extension* extension) {
  const Application* application = FindApplication(extension);
  if (application)
    return application->icon_.get();
  AssociateApplicationData(extension);
  return NULL;
}

int BackgroundApplicationListModel::GetPosition(
    const Extension* extension) const {
  int position = 0;
  const std::string& id = extension->id();
  for (ExtensionList::const_iterator cursor = extensions_.begin();
       cursor != extensions_.end();
       ++cursor, ++position) {
    if (id == cursor->get()->id())
      return position;
  }
  NOTREACHED();
  return -1;
}

// static
bool BackgroundApplicationListModel::IsBackgroundApp(
    const Extension& extension) {
  return HasBackgroundAppPermission(extension.api_permissions());
}

void BackgroundApplicationListModel::Observe(
    NotificationType type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  if (type == NotificationType::EXTENSIONS_READY) {
    Update();
    return;
  }
  ExtensionService* service = profile_->GetExtensionService();
  if (!service || !service->is_ready())
    return;

  switch (type.value) {
    case NotificationType::EXTENSION_LOADED:
      OnExtensionLoaded(Details<Extension>(details).ptr());
      break;
    case NotificationType::EXTENSION_UNLOADED:
      OnExtensionUnloaded(Details<UnloadedExtensionInfo>(details)->extension);
      break;
    default:
      NOTREACHED() << "Received unexpected notification";
  }
}

void BackgroundApplicationListModel::OnApplicationDataChanged(
    const Extension* extension) {
  FOR_EACH_OBSERVER(Observer, observers_, OnApplicationDataChanged(extension));
}

void BackgroundApplicationListModel::OnExtensionLoaded(Extension* extension) {
  // We only care about extensions that are background applications
  if (!IsBackgroundApp(*extension))
    return;
  AssociateApplicationData(extension);
  Update();
}

void BackgroundApplicationListModel::OnExtensionUnloaded(
    const Extension* extension) {
  if (!IsBackgroundApp(*extension))
    return;
  Update();
  DissociateApplicationData(extension);
}

void BackgroundApplicationListModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// Update queries the extensions service of the profile with which the model was
// initialized to determine the current set of background applications.  If that
// differs from the old list, it generates OnApplicationListChanged events for
// each observer.
void BackgroundApplicationListModel::Update() {
  ExtensionService* service = profile_->GetExtensionService();

  // Discover current background applications, compare with previous list, which
  // is consistently sorted, and notify observers if they differ.
  ExtensionList extensions;
  GetServiceApplications(service, &extensions);
  ExtensionList::const_iterator old_cursor = extensions_.begin();
  ExtensionList::const_iterator new_cursor = extensions.begin();
  while (old_cursor != extensions_.end() &&
         new_cursor != extensions.end() &&
         (*old_cursor)->name() == (*new_cursor)->name() &&
         (*old_cursor)->id() == (*new_cursor)->id()) {
    ++old_cursor;
    ++new_cursor;
  }
  if (old_cursor != extensions_.end() || new_cursor != extensions.end()) {
    extensions_ = extensions;
    FOR_EACH_OBSERVER(Observer, observers_, OnApplicationListChanged());
  }
}
