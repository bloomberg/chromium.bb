// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_application_list_model.h"

#include <algorithm>
#include <set>

#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

using extensions::APIPermission;
using extensions::Extension;
using extensions::ExtensionList;
using extensions::ExtensionRegistry;
using extensions::ExtensionSet;
using extensions::PermissionSet;
using extensions::UnloadedExtensionInfo;
using extensions::UpdatedExtensionPermissionsInfo;

class ExtensionNameComparator {
 public:
  explicit ExtensionNameComparator(icu::Collator* collator);
  bool operator()(const scoped_refptr<const Extension>& x,
                  const scoped_refptr<const Extension>& y);

 private:
  icu::Collator* collator_;
};

ExtensionNameComparator::ExtensionNameComparator(icu::Collator* collator)
  : collator_(collator) {
}

bool ExtensionNameComparator::operator()(
    const scoped_refptr<const Extension>& x,
    const scoped_refptr<const Extension>& y) {
  return l10n_util::StringComparator<base::string16>(collator_)(
      base::UTF8ToUTF16(x->name()), base::UTF8ToUTF16(y->name()));
}

// Background application representation, private to the
// BackgroundApplicationListModel class.
class BackgroundApplicationListModel::Application
    : public base::SupportsWeakPtr<Application> {
 public:
  Application(BackgroundApplicationListModel* model,
              const Extension* an_extension);

  virtual ~Application();

  // Invoked when a request icon is available.
  void OnImageLoaded(const gfx::Image& image);

  // Uses the FILE thread to request this extension's icon, sized
  // appropriately.
  void RequestIcon(extension_misc::ExtensionIcons size);

  const Extension* extension_;
  scoped_ptr<gfx::ImageSkia> icon_;
  BackgroundApplicationListModel* model_;
};

namespace {
void GetServiceApplications(ExtensionService* service,
                            ExtensionList* applications_result) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(service->profile());
  const ExtensionSet& enabled_extensions = registry->enabled_extensions();

  for (ExtensionSet::const_iterator cursor = enabled_extensions.begin();
       cursor != enabled_extensions.end();
       ++cursor) {
    const Extension* extension = cursor->get();
    if (BackgroundApplicationListModel::IsBackgroundApp(*extension,
                                                        service->profile())) {
      applications_result->push_back(extension);
    }
  }

  // Walk the list of terminated extensions also (just because an extension
  // crashed doesn't mean we should ignore it).
  const ExtensionSet& terminated_extensions = registry->terminated_extensions();
  for (ExtensionSet::const_iterator cursor = terminated_extensions.begin();
       cursor != terminated_extensions.end();
       ++cursor) {
    const Extension* extension = cursor->get();
    if (BackgroundApplicationListModel::IsBackgroundApp(*extension,
                                                        service->profile())) {
      applications_result->push_back(extension);
    }
  }

  std::string locale = g_browser_process->GetApplicationLocale();
  icu::Locale loc(locale.c_str());
  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(icu::Collator::createInstance(loc, error));
  std::sort(applications_result->begin(), applications_result->end(),
       ExtensionNameComparator(collator.get()));
}

}  // namespace

void
BackgroundApplicationListModel::Observer::OnApplicationDataChanged(
    const Extension* extension, Profile* profile) {
}

void
BackgroundApplicationListModel::Observer::OnApplicationListChanged(
    Profile* profile) {
}

BackgroundApplicationListModel::Observer::~Observer() {
}

BackgroundApplicationListModel::Application::~Application() {
}

BackgroundApplicationListModel::Application::Application(
    BackgroundApplicationListModel* model,
    const Extension* extension)
    : extension_(extension), model_(model) {}

void BackgroundApplicationListModel::Application::OnImageLoaded(
    const gfx::Image& image) {
  if (image.IsEmpty())
    return;
  icon_.reset(image.CopyImageSkia());
  model_->SendApplicationDataChangedNotifications(extension_);
}

void BackgroundApplicationListModel::Application::RequestIcon(
    extension_misc::ExtensionIcons size) {
  extensions::ExtensionResource resource =
      extensions::IconsInfo::GetIconResource(
          extension_, size, ExtensionIconSet::MATCH_BIGGER);
  extensions::ImageLoader::Get(model_->profile_)->LoadImageAsync(
      extension_, resource, gfx::Size(size, size),
      base::Bind(&Application::OnImageLoaded, AsWeakPtr()));
}

BackgroundApplicationListModel::~BackgroundApplicationListModel() {
  STLDeleteContainerPairSecondPointers(applications_.begin(),
                                       applications_.end());
}

BackgroundApplicationListModel::BackgroundApplicationListModel(Profile* profile)
    : profile_(profile),
      ready_(false) {
  DCHECK(profile_);
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                 content::Source<Profile>(profile));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<Profile>(profile));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                 content::Source<Profile>(profile));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
                 content::Source<Profile>(profile));
  registrar_.Add(this,
                 chrome::NOTIFICATION_BACKGROUND_CONTENTS_SERVICE_CHANGED,
                 content::Source<Profile>(profile));
  ExtensionService* service = extensions::ExtensionSystem::Get(profile)->
      extension_service();
  if (service && service->is_ready()) {
    Update();
    ready_ = true;
  }
}

void BackgroundApplicationListModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BackgroundApplicationListModel::AssociateApplicationData(
    const Extension* extension) {
  DCHECK(IsBackgroundApp(*extension, profile_));
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
    Update();
    application->RequestIcon(extension_misc::EXTENSION_ICON_BITTY);
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
  return extensions_[position].get();
}

const BackgroundApplicationListModel::Application*
BackgroundApplicationListModel::FindApplication(
    const Extension* extension) const {
  const std::string& id = extension->id();
  ApplicationMap::const_iterator found = applications_.find(id);
  return (found == applications_.end()) ? NULL : found->second;
}

BackgroundApplicationListModel::Application*
BackgroundApplicationListModel::FindApplication(
    const Extension* extension) {
  const std::string& id = extension->id();
  ApplicationMap::iterator found = applications_.find(id);
  return (found == applications_.end()) ? NULL : found->second;
}

const gfx::ImageSkia* BackgroundApplicationListModel::GetIcon(
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
bool BackgroundApplicationListModel::RequiresBackgroundModeForPushMessaging(
    const Extension& extension) {
  // No PushMessaging permission - does not require the background mode.
  if (!extension.permissions_data()->HasAPIPermission(
          APIPermission::kPushMessaging)) {
    return false;
  }

  // If in the whitelist, then does not require background mode even if
  // uses push messaging.
  // TODO(dimich): remove this whitelist once we have a better way to keep
  // listening for GCM. http://crbug.com/311268
  std::string id_hash = base::SHA1HashString(extension.id());
  std::string hexencoded_id_hash = base::HexEncode(id_hash.c_str(),
                                                   id_hash.length());
  // The id starting from "9A04..." is a one from unit test.
  if (hexencoded_id_hash == "C41AD9DCD670210295614257EF8C9945AD68D86E" ||
      hexencoded_id_hash == "9A0417016F345C934A1A88F55CA17C05014EEEBA")
     return false;

   return true;
 }

// static
bool BackgroundApplicationListModel::IsBackgroundApp(
    const Extension& extension, Profile* profile) {
  // An extension is a "background app" if it has the "background API"
  // permission, and meets one of the following criteria:
  // 1) It is an extension (not a hosted app).
  // 2) It is a hosted app, and has a background contents registered or in the
  //    manifest.

  // Ephemeral apps are denied any background activity after their event page
  // has been destroyed, thus they cannot be background apps.
  if (extensions::util::IsEphemeralApp(extension.id(), profile))
    return false;

  // Not a background app if we don't have the background permission or
  // the push messaging permission
  if (!extension.permissions_data()->HasAPIPermission(
          APIPermission::kBackground) &&
      !RequiresBackgroundModeForPushMessaging(extension))
    return false;

  // Extensions and packaged apps with background permission are always treated
  // as background apps.
  if (!extension.is_hosted_app())
    return true;

  // Hosted apps with manifest-provided background pages are background apps.
  if (extensions::BackgroundInfo::HasBackgroundPage(&extension))
    return true;

  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(profile);
  base::string16 app_id = base::ASCIIToUTF16(extension.id());
  // If we have an active or registered background contents for this app, then
  // it's a background app. This covers the cases where the app has created its
  // background contents, but it hasn't navigated yet, or the background
  // contents crashed and hasn't yet been restarted - in both cases we still
  // want to treat the app as a background app.
  if (service->GetAppBackgroundContents(app_id) ||
      service->HasRegisteredBackgroundContents(app_id)) {
    return true;
  }

  // Doesn't meet our criteria, so it's not a background app.
  return false;
}

void BackgroundApplicationListModel::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED) {
    Update();
    ready_ = true;
    return;
  }
  ExtensionService* service = extensions::ExtensionSystem::Get(profile_)->
      extension_service();
  if (!service || !service->is_ready())
    return;

  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED:
      OnExtensionLoaded(content::Details<Extension>(details).ptr());
      break;
    case extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED:
      OnExtensionUnloaded(
          content::Details<UnloadedExtensionInfo>(details)->extension);
      break;
    case extensions::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED:
      OnExtensionPermissionsUpdated(
          content::Details<UpdatedExtensionPermissionsInfo>(details)->extension,
          content::Details<UpdatedExtensionPermissionsInfo>(details)->reason,
          content::Details<UpdatedExtensionPermissionsInfo>(details)->
              permissions);
      break;
    case chrome::NOTIFICATION_BACKGROUND_CONTENTS_SERVICE_CHANGED:
      Update();
      break;
    default:
      NOTREACHED() << "Received unexpected notification";
  }
}

void BackgroundApplicationListModel::SendApplicationDataChangedNotifications(
    const Extension* extension) {
  FOR_EACH_OBSERVER(Observer, observers_, OnApplicationDataChanged(extension,
                                                                   profile_));
}

void BackgroundApplicationListModel::OnExtensionLoaded(
    const Extension* extension) {
  // We only care about extensions that are background applications
  if (!IsBackgroundApp(*extension, profile_))
    return;
  AssociateApplicationData(extension);
}

void BackgroundApplicationListModel::OnExtensionUnloaded(
    const Extension* extension) {
  if (!IsBackgroundApp(*extension, profile_))
    return;
  Update();
  DissociateApplicationData(extension);
}

void BackgroundApplicationListModel::OnExtensionPermissionsUpdated(
    const Extension* extension,
    UpdatedExtensionPermissionsInfo::Reason reason,
    const PermissionSet* permissions) {
  if (permissions->HasAPIPermission(APIPermission::kBackground)) {
    switch (reason) {
      case UpdatedExtensionPermissionsInfo::ADDED:
        DCHECK(IsBackgroundApp(*extension, profile_));
        OnExtensionLoaded(extension);
        break;
      case UpdatedExtensionPermissionsInfo::REMOVED:
        DCHECK(!IsBackgroundApp(*extension, profile_));
        Update();
        DissociateApplicationData(extension);
        break;
      default:
        NOTREACHED();
    }
  }
}

void BackgroundApplicationListModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// Update queries the extensions service of the profile with which the model was
// initialized to determine the current set of background applications.  If that
// differs from the old list, it generates OnApplicationListChanged events for
// each observer.
void BackgroundApplicationListModel::Update() {
  ExtensionService* service = extensions::ExtensionSystem::Get(profile_)->
      extension_service();

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
    FOR_EACH_OBSERVER(Observer, observers_, OnApplicationListChanged(profile_));
  }
}
