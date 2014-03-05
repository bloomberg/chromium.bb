// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_LOCATION_LOCATION_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

class Profile;

namespace content {
class BrowserContext;
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
class LocationManager : public BrowserContextKeyedAPI,
                        public content::NotificationObserver,
                        public base::SupportsWeakPtr<LocationManager> {
 public:
  explicit LocationManager(content::BrowserContext* context);
  virtual ~LocationManager();

  // Adds location request for the given extension, and starts the location
  // tracking.
  void AddLocationRequest(
      const std::string& extension_id,
      const std::string& request_name,
      const double* distance_update_threshold_meters,
      const double* time_between_updates_ms);

  // Cancels and removes the request with the given |name| for the given
  // extension.
  void RemoveLocationRequest(const std::string& extension_id,
                             const std::string& name);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<LocationManager>* GetFactoryInstance();

  // Convenience method to get the LocationManager for a profile.
  static LocationManager* Get(content::BrowserContext* context);

 private:
  friend class LocationRequest;
  friend class BrowserContextKeyedAPIFactory<LocationManager>;

  typedef std::string ExtensionId;
  typedef scoped_refptr<LocationRequest> LocationRequestPointer;
  typedef std::multimap<ExtensionId, LocationRequestPointer> LocationRequestMap;
  typedef LocationRequestMap::iterator LocationRequestIterator;

  // Converts |position| from GeolocationProvider to the location API
  // |coordinates|.
  static void GeopositionToApiCoordinates(
      const content::Geoposition& position,
      api::location::Coordinates* coordinates);

  // Sends a location update to the extension.
  void SendLocationUpdate(const std::string& extension_id,
                          const std::string& request_name,
                          const content::Geoposition& position);

  // NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "LocationManager"; }

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
