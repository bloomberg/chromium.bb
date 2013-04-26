// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace content {
struct Geoposition;
}  // namespace content

namespace extensions {
class LocationManager;
class LocationRequest;

namespace api {
namespace location {

struct Coordinates;

}  // namespace location
}  // namespace api

// Profile's manager of all location watch requests created by chrome.location
// API. Lives in the UI thread.
class LocationManager
    : public content::NotificationObserver,
      public base::SupportsWeakPtr<LocationManager> {
 public:
  explicit LocationManager(Profile* profile);
  virtual ~LocationManager();

  // Adds location request for the given extension, and starts the location
  // tracking.
  void AddLocationRequest(const std::string& extension_id,
                          const std::string& request_name);

  // Cancels and removes the request with the given |name| for the given
  // extension.
  void RemoveLocationRequest(const std::string& extension_id,
                             const std::string& name);

 private:
  friend class LocationRequest;

  typedef scoped_refptr<LocationRequest> LocationRequestPointer;
  typedef std::vector<LocationRequestPointer> LocationRequestList;
  typedef std::string ExtensionId;

  // TODO(vadimt): Consider converting to multimap.
  typedef std::map<ExtensionId, LocationRequestList> LocationRequestMap;

  // Iterator used to identify a particular request within the Map/List pair.
  // "Not found" is represented by <location_requests_.end(), invalid_iterator>.
  typedef std::pair<LocationRequestMap::iterator, LocationRequestList::iterator>
      LocationRequestIterator;

  // Helper to return the iterators within the LocationRequestMap and
  // LocationRequestList for the  matching request, or an iterator to the end of
  // the LocationRequestMap if none were found.
  LocationRequestIterator GetLocationRequestIterator(
      const std::string& extension_id,
      const std::string& name);

  // Converts |position| from GeolocationProvider to the location API
  // |coordinates|.
  static void GeopositionToApiCoordinates(
      const content::Geoposition& position,
      api::location::Coordinates* coordinates);

  // Helper to cancel and remove the request at the given iterator. The iterator
  // must be valid.
  void RemoveLocationRequestIterator(const LocationRequestIterator& iter);

  // Sends a location update to the extension.
  void SendLocationUpdate(const std::string& extension_id,
                          const std::string& request_name,
                          const content::Geoposition& position);

  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Profile for this location manager.
  Profile* const profile_;

  // A map of our pending location requests, per extension.
  // Invariant: None of the LocationRequestLists are empty.
  LocationRequestMap location_requests_;

  // Used for tracking registrations to profile's extensions events.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(LocationManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_MANAGER_H_
