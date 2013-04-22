// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/extensions/api/location/location_manager.h"

#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/location.h"
#include "content/browser/geolocation/geolocation_provider.h"

// TODO(vadimt): Add tests.
// TODO(vadimt): Remove requests when the extension uninstalls.
namespace extensions {

namespace location = api::location;

LocationRequest::LocationRequest(
    const std::string& request_name,
    const location::WatchLocationRequestInfo& request_info)
    : name(request_name) {
  // TODO(vadimt): use request_info.
}

void LocationManagerTraits::Destruct(const LocationManager* location_manager) {
  location_manager->OnDestruct();
}

LocationManager::LocationManager(Profile* profile)
    : profile_(profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void LocationManager::AddLocationRequest(
    const std::string& extension_id,
    const LocationRequest& location_request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(vadimt): Consider resuming requests after restarting the browser.

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&LocationManager::AddLocationRequestOnIOThread,
                 this,
                 extension_id,
                 location_request));
}

void LocationManager::RemoveLocationRequest(const std::string& extension_id,
                                            const std::string& name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&LocationManager::RemoveLocationRequestOnIOThread,
                 this,
                 extension_id,
                 name));
}

void LocationManager::OnLocationUpdate(const content::Geoposition& position) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&LocationManager::OnLocationUpdateOnUIThread,
                 this,
                 position,
                 location_requests_));
}

LocationManager::~LocationManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!location_requests_.empty())
    content::GeolocationProvider::GetInstance()->RemoveObserver(this);
}

LocationManager::LocationRequestIterator
    LocationManager::GetLocationRequestIterator(
        const std::string& extension_id,
        const std::string& name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  LocationRequestMap::iterator list = location_requests_.find(extension_id);
  if (list == location_requests_.end())
    return make_pair(location_requests_.end(), LocationRequestList::iterator());

  for (LocationRequestList::iterator it = list->second.begin();
       it != list->second.end(); ++it) {
    if (it->name == name)
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  LocationRequestList& list = iter.first->second;
  list.erase(iter.second);
  if (list.empty())
    location_requests_.erase(iter.first);
}

void LocationManager::AddLocationRequestOnIOThread(
    const std::string& extension_id,
    const LocationRequest& location_request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (location_requests_.empty()) {
    content::GeolocationObserverOptions options(true);
    // TODO(vadimt): Decide whether each request should be an observer.
    content::GeolocationProvider::GetInstance()->AddObserver(this, options);
  }

  // Override any old request with the same name.
  LocationRequestIterator old_location_request =
      GetLocationRequestIterator(extension_id, location_request.name);
  if (old_location_request.first != location_requests_.end())
    RemoveLocationRequestIterator(old_location_request);

  location_requests_[extension_id].push_back(location_request);
}

void LocationManager::RemoveLocationRequestOnIOThread(
    const std::string& extension_id,
    const std::string& name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  LocationRequestIterator it = GetLocationRequestIterator(extension_id, name);
  if (it.first == location_requests_.end())
    return;

  RemoveLocationRequestIterator(it);

  if (location_requests_.empty())
    content::GeolocationProvider::GetInstance()->RemoveObserver(this);
}

void LocationManager::OnLocationUpdateOnUIThread(
    const content::Geoposition& position,
    LocationRequestMap const& location_requests) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(vadimt): Set location.request_names.
  scoped_ptr<ListValue> args(new ListValue());
  std::string event_name;

  if (position.Validate() &&
      position.error_code == content::Geoposition::ERROR_CODE_NONE) {
    // Set data for onLocationUpdate event.
    location::Location location;
    GeopositionToApiCoordinates(position, &location.coords);
    location.timestamp = position.timestamp.ToJsTime();

    args->Append(location.ToValue().release());
    event_name = "location.onLocationUpdate";
  } else {
    // Set data for onLocationError event.
    args->AppendString(position.error_message);
    event_name = "location.onLocationError";
  }

  scoped_ptr<Event> event(new Event(event_name, args.Pass()));

  // Dispatch the event to all extensions that watch location.
  for (LocationRequestMap::const_iterator it = location_requests.begin();
       it != location_requests.end();
       ++it) {
    ExtensionSystem::Get(profile_)->event_router()->
        DispatchEventToExtension(it->first, event.Pass());
  }
}

void LocationManager::OnDestruct() const {
  if (!BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this))
    NOTREACHED();
}

}  // namespace extensions
