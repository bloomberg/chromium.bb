// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/location/location_manager.h"

#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/location.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "content/browser/geolocation/geolocation_provider.h"

// TODO(vadimt): Add tests.
namespace extensions {

namespace location = api::location;

// Request created by chrome.location.watchLocation() call.
// Lives in the IO thread, except for the constructor.
class LocationRequest
    : public content::GeolocationObserver,
      public base::RefCountedThreadSafe<LocationRequest,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  LocationRequest(
      const base::WeakPtr<LocationManager>& location_manager,
      const std::string& extension_id,
      const std::string& request_name);

  const std::string& request_name() const { return request_name_; }

  // Grants permission for using geolocation.
  static void GrantPermission();

 private:
  friend class base::DeleteHelper<LocationRequest>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;

  virtual ~LocationRequest();

  void AddObserverOnIOThread();

  // GeolocationObserver
  virtual void OnLocationUpdate(const content::Geoposition& position) OVERRIDE;

  // Request name.
  const std::string request_name_;

  // Id of the owner extension.
  const std::string extension_id_;

  // Owning location manager.
  const base::WeakPtr<LocationManager> location_manager_;

  DISALLOW_COPY_AND_ASSIGN(LocationRequest);
};

LocationRequest::LocationRequest(
    const base::WeakPtr<LocationManager>& location_manager,
    const std::string& extension_id,
    const std::string& request_name)
    : request_name_(request_name),
      extension_id_(extension_id),
      location_manager_(location_manager) {
  // TODO(vadimt): use request_info.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&LocationRequest::AddObserverOnIOThread,
                 this));
}

void LocationRequest::GrantPermission() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  content::GeolocationProvider::GetInstance()->OnPermissionGranted();
}

LocationRequest::~LocationRequest() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  content::GeolocationProvider::GetInstance()->RemoveObserver(this);
}

void LocationRequest::AddObserverOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  content::GeolocationObserverOptions options(true);
  // TODO(vadimt): This can get a location cached by GeolocationProvider,
  // contrary to the API definition which says that creating a location watch
  // will get new location.
  content::GeolocationProvider::GetInstance()->AddObserver(this, options);
}

void LocationRequest::OnLocationUpdate(const content::Geoposition& position) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&LocationManager::SendLocationUpdate,
                 location_manager_,
                 extension_id_,
                 request_name_,
                 position));
}

LocationManager::LocationManager(Profile* profile)
    : profile_(profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
}

void LocationManager::AddLocationRequest(const std::string& extension_id,
                                         const std::string& request_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(vadimt): Consider resuming requests after restarting the browser.

  // Override any old request with the same name.
  LocationRequestIterator old_location_request =
      GetLocationRequestIterator(extension_id, request_name);
  if (old_location_request.first != location_requests_.end())
    RemoveLocationRequestIterator(old_location_request);

  LocationRequestPointer location_request = new LocationRequest(AsWeakPtr(),
                                                                extension_id,
                                                                request_name);
  location_requests_[extension_id].push_back(location_request);
}

void LocationManager::RemoveLocationRequest(const std::string& extension_id,
                                            const std::string& name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LocationRequestIterator it = GetLocationRequestIterator(extension_id, name);
  if (it.first == location_requests_.end())
    return;

  RemoveLocationRequestIterator(it);
}

LocationManager::~LocationManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

LocationManager::LocationRequestIterator
    LocationManager::GetLocationRequestIterator(
        const std::string& extension_id,
        const std::string& name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LocationRequestMap::iterator list = location_requests_.find(extension_id);
  if (list == location_requests_.end())
    return make_pair(location_requests_.end(), LocationRequestList::iterator());

  for (LocationRequestList::iterator it = list->second.begin();
       it != list->second.end(); ++it) {
    if ((*it)->request_name() == name)
      return make_pair(list, it);
  }

  return make_pair(location_requests_.end(), LocationRequestList::iterator());
}

void LocationManager::GeopositionToApiCoordinates(
      const content::Geoposition& position,
      location::Coordinates* coordinates) {
  coordinates->latitude = position.latitude;
  coordinates->longitude = position.longitude;
  if (position.altitude > -10000.)
    coordinates->altitude.reset(new double(position.altitude));
  coordinates->accuracy = position.accuracy;
  if (position.altitude_accuracy >= 0.) {
    coordinates->altitude_accuracy.reset(
        new double(position.altitude_accuracy));
  }
  if (position.heading >= 0. && position.heading <= 360.)
    coordinates->heading.reset(new double(position.heading));
  if (position.speed >= 0.)
    coordinates->speed.reset(new double(position.speed));
}

void LocationManager::RemoveLocationRequestIterator(
    const LocationRequestIterator& iter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  LocationRequestList& list = iter.first->second;
  list.erase(iter.second);
  if (list.empty())
    location_requests_.erase(iter.first);
}

void LocationManager::SendLocationUpdate(
    const std::string& extension_id,
    const std::string& request_name,
    const content::Geoposition& position) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<ListValue> args(new ListValue());
  std::string event_name;

  if (position.Validate() &&
      position.error_code == content::Geoposition::ERROR_CODE_NONE) {
    // Set data for onLocationUpdate event.
    location::Location location;
    location.name = request_name;
    GeopositionToApiCoordinates(position, &location.coords);
    location.timestamp = position.timestamp.ToJsTime();

    args->Append(location.ToValue().release());
    event_name = "location.onLocationUpdate";
  } else {
    // Set data for onLocationError event.
    // TODO(vadimt): Set name.
    args->AppendString(position.error_message);
    event_name = "location.onLocationError";
  }

  scoped_ptr<Event> event(new Event(event_name, args.Pass()));

  ExtensionSystem::Get(profile_)->event_router()->
      DispatchEventToExtension(extension_id, event.Pass());
}

void LocationManager::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      // Grants permission to use geolocation once an extension with "location"
      // permission is loaded.
      const Extension* extension =
          content::Details<const Extension>(details).ptr();

      if (extension->HasAPIPermission(APIPermission::kLocation)) {
          BrowserThread::PostTask(
              BrowserThread::IO,
              FROM_HERE,
              base::Bind(&LocationRequest::GrantPermission));
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      // Delete all requests from the unloaded extension.
      const Extension* extension =
          content::Details<const UnloadedExtensionInfo>(details)->extension;
      location_requests_.erase(extension->id());
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace extensions
