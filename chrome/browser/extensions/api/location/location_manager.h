// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/browser/geolocation/geolocation_observer.h"

class Profile;

namespace content {
struct Geoposition;
}  // namespace content

namespace extensions {
class LocationManager;

namespace api {
namespace location {

struct Coordinates;
struct WatchLocationRequestInfo;

}  // namespace location
}  // namespace api

// Data for a location watch request created by chrome.location.watchLocation().
struct LocationRequest {
  // Request name
  std::string name;

  LocationRequest(
      const std::string& request_name,
      const api::location::WatchLocationRequestInfo& request_info);
};

// Traits for LocationManager to delete it in the IO thread.
struct LocationManagerTraits {
  static void Destruct(const LocationManager* location_manager);
};

// Profile's manager of all location watch requests created by chrome.location
// API.
class LocationManager
    : public content::GeolocationObserver,
      public base::RefCountedThreadSafe<LocationManager,
                                        LocationManagerTraits> {
 public:
  explicit LocationManager(Profile* profile);

  // Adds |location_request| for the given |extension|, and starts the location
  // tracking.
  void AddLocationRequest(
      const std::string& extension_id,
      const LocationRequest& location_request);

  // Cancels and removes the request with the given |name| for the given
  // |extension|.
  void RemoveLocationRequest(const std::string& extension_id,
                             const std::string& name);

  // GeolocationObserver
  virtual void OnLocationUpdate(const content::Geoposition& position) OVERRIDE;

 private:
  friend struct LocationManagerTraits;
  friend class base::DeleteHelper<LocationManager>;

  typedef std::vector<LocationRequest> LocationRequestList;
  typedef std::string ExtensionId;

  // TODO(vadimt): Consider converting to multimap.
  typedef std::map<ExtensionId, LocationRequestList> LocationRequestMap;

  // Iterator used to identify a particular request within the Map/List pair.
  // "Not found" is represented by <location_requests_.end(), invalid_iterator>.
  typedef std::pair<LocationRequestMap::iterator, LocationRequestList::iterator>
      LocationRequestIterator;

  virtual ~LocationManager();

  // Helper to return the iterators within the LocationRequestMap and
  // LocationRequestList for the  matching request, or an iterator to the end of
  // the LocationRequestMap if none were found.
  LocationRequestIterator GetLocationRequestIterator(
      const std::string& extension_id,
      const std::string& name);

  // Converts |position| from GeolocationProvider to the location API
  // |coordinates|.
  void GeopositionToApiCoordinates(
      const content::Geoposition& position,
      api::location::Coordinates* coordinates);

  // Helper to cancel and remove the request at the given iterator. The iterator
  // must be valid.
  void RemoveLocationRequestIterator(const LocationRequestIterator& iter);

  void AddLocationRequestOnIOThread(const std::string& extension_id,
                                    const LocationRequest& location_request);
  void RemoveLocationRequestOnIOThread(const std::string& extension_id,
                                       const std::string& name);
  void OnLocationUpdateOnUIThread(const content::Geoposition& position,
                                  const LocationRequestMap& location_requests);

  // Deteled 'this' in IO thread once refcount reaches 0.
  void OnDestruct() const;

  // Profile for this location manager.
  // Must be accessed only from the UI thread.
  Profile* const profile_;

  // A map of our pending location requests, per extension.
  // Invariant: None of the LocationRequestLists are empty.
  // Must only be accessed from the IO thread.
  LocationRequestMap location_requests_;

  DISALLOW_COPY_AND_ASSIGN(LocationManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_MANAGER_H_
