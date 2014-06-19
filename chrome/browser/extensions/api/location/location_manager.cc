// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/location/location_manager.h"

#include <math.h>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "chrome/common/extensions/api/location.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/common/geoposition.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"

using content::BrowserThread;

// TODO(vadimt): Add tests.
namespace extensions {

namespace location = api::location;

namespace updatepolicy {

// Base class for all update policies for sending a location.
class UpdatePolicy : public base::RefCounted<UpdatePolicy> {
 public:
  explicit UpdatePolicy() {}

  // True if the caller should send an update based off of this policy.
  virtual bool ShouldSendUpdate(const content::Geoposition&) const = 0;

  // Updates any policy state on reporting a position.
  virtual void OnPositionReported(const content::Geoposition&) = 0;

 protected:
  virtual ~UpdatePolicy() {}

 private:
  friend class base::RefCounted<UpdatePolicy>;
  DISALLOW_COPY_AND_ASSIGN(UpdatePolicy);
};

// A policy that controls sending an update below a distance threshold.
class DistanceBasedUpdatePolicy : public UpdatePolicy {
 public:
  explicit DistanceBasedUpdatePolicy(double distance_update_threshold_meters) :
      distance_update_threshold_meters_(distance_update_threshold_meters)
  {}

  // UpdatePolicy Implementation
  virtual bool ShouldSendUpdate(const content::Geoposition& position) const
      OVERRIDE {
    return !last_updated_position_.Validate() ||
        Distance(position.latitude,
                 position.longitude,
                 last_updated_position_.latitude,
                 last_updated_position_.longitude) >
            distance_update_threshold_meters_;
  }

  virtual void OnPositionReported(const content::Geoposition& position)
      OVERRIDE {
    last_updated_position_ = position;
  }

 private:
  virtual ~DistanceBasedUpdatePolicy() {}

  // Calculates the distance between two latitude and longitude points.
  static double Distance(const double latitude1,
                         const double longitude1,
                         const double latitude2,
                         const double longitude2) {
    // The earth has a radius of about 6371 km.
    const double kRadius = 6371000;
    const double kPi = 3.14159265358979323846;
    const double kDegreesToRadians = kPi / 180.0;

    // Conversions
    const double latitude1Rad = latitude1 * kDegreesToRadians;
    const double latitude2Rad = latitude2 * kDegreesToRadians;
    const double latitudeDistRad = latitude2Rad - latitude1Rad;
    const double longitudeDistRad = (longitude2 - longitude1) *
                                    kDegreesToRadians;

    // The Haversine Formula determines the great circle distance
    // between two points on a sphere.
    const double chordLengthSquared = pow(sin(latitudeDistRad / 2.0), 2) +
                                      (pow(sin(longitudeDistRad / 2.0), 2) *
                                       cos(latitude1Rad) *
                                       cos(latitude2Rad));
    const double angularDistance = 2.0 * atan2(sqrt(chordLengthSquared),
                                               sqrt(1.0 - chordLengthSquared));
    return kRadius * angularDistance;
  }

  const double distance_update_threshold_meters_;
  content::Geoposition last_updated_position_;

  DISALLOW_COPY_AND_ASSIGN(DistanceBasedUpdatePolicy);
};

// A policy that controls sending an update above a time threshold.
class TimeBasedUpdatePolicy : public UpdatePolicy {
 public:
  explicit TimeBasedUpdatePolicy(double time_between_updates_ms) :
      time_between_updates_ms_(time_between_updates_ms)
  {}

  // UpdatePolicy Implementation
  virtual bool ShouldSendUpdate(const content::Geoposition&) const OVERRIDE {
    return (base::Time::Now() - last_update_time_).InMilliseconds() >
        time_between_updates_ms_;
  }

  virtual void OnPositionReported(const content::Geoposition&) OVERRIDE {
    last_update_time_ = base::Time::Now();
  }

 private:
  virtual ~TimeBasedUpdatePolicy() {}

  base::Time last_update_time_;
  const double time_between_updates_ms_;

  DISALLOW_COPY_AND_ASSIGN(TimeBasedUpdatePolicy);
};

} // namespace updatepolicy

// Request created by chrome.location.watchLocation() call.
class LocationRequest : public base::RefCounted<LocationRequest> {
 public:
  LocationRequest(
      LocationManager* location_manager,
      const std::string& extension_id,
      const std::string& request_name,
      const double* distance_update_threshold_meters,
      const double* time_between_updates_ms);

  const std::string& request_name() const { return request_name_; }

 private:
  friend class base::RefCounted<LocationRequest>;
   ~LocationRequest();

  void OnLocationUpdate(const content::Geoposition& position);

  // Determines if all policies say to send a position update.
  // If there are no policies, this always says yes.
  bool ShouldSendUpdate(const content::Geoposition& position);

  // Updates the policies on sending a position update.
  void OnPositionReported(const content::Geoposition& position);

  // Request name.
  const std::string request_name_;

  // Id of the owner extension.
  const std::string extension_id_;

  // Owning location manager.
  LocationManager* location_manager_;

  // Holds Update Policies.
  typedef std::vector<scoped_refptr<updatepolicy::UpdatePolicy> >
      UpdatePolicyVector;
  UpdatePolicyVector update_policies_;

  scoped_ptr<content::GeolocationProvider::Subscription>
      geolocation_subscription_;

  DISALLOW_COPY_AND_ASSIGN(LocationRequest);
};

LocationRequest::LocationRequest(
    LocationManager* location_manager,
    const std::string& extension_id,
    const std::string& request_name,
    const double* distance_update_threshold_meters,
    const double* time_between_updates_ms)
    : request_name_(request_name),
      extension_id_(extension_id),
      location_manager_(location_manager) {
  // TODO(vadimt): use request_info.
  if (time_between_updates_ms) {
    update_policies_.push_back(
        new updatepolicy::TimeBasedUpdatePolicy(
            *time_between_updates_ms));
  }

  if (distance_update_threshold_meters) {
    update_policies_.push_back(
        new updatepolicy::DistanceBasedUpdatePolicy(
              *distance_update_threshold_meters));
  }

  // TODO(vadimt): This can get a location cached by GeolocationProvider,
  // contrary to the API definition which says that creating a location watch
  // will get new location.
  geolocation_subscription_ = content::GeolocationProvider::GetInstance()->
      AddLocationUpdateCallback(
          base::Bind(&LocationRequest::OnLocationUpdate,
                     base::Unretained(this)),
          true);
}

LocationRequest::~LocationRequest() {
}

void LocationRequest::OnLocationUpdate(const content::Geoposition& position) {
  if (ShouldSendUpdate(position)) {
    OnPositionReported(position);
    location_manager_->SendLocationUpdate(
        extension_id_, request_name_, position);
  }
}

bool LocationRequest::ShouldSendUpdate(const content::Geoposition& position) {
  for (UpdatePolicyVector::iterator it = update_policies_.begin();
       it != update_policies_.end();
       ++it) {
    if (!((*it)->ShouldSendUpdate(position))) {
      return false;
    }
  }
  return true;
}

void LocationRequest::OnPositionReported(const content::Geoposition& position) {
  for (UpdatePolicyVector::iterator it = update_policies_.begin();
       it != update_policies_.end();
       ++it) {
    (*it)->OnPositionReported(position);
  }
}

LocationManager::LocationManager(content::BrowserContext* context)
    : browser_context_(context), extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

void LocationManager::AddLocationRequest(
    const std::string& extension_id,
    const std::string& request_name,
    const double* distance_update_threshold_meters,
    const double* time_between_updates_ms) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(vadimt): Consider resuming requests after restarting the browser.

  // Override any old request with the same name.
  RemoveLocationRequest(extension_id, request_name);

  LocationRequestPointer location_request =
      new LocationRequest(this,
                          extension_id,
                          request_name,
                          distance_update_threshold_meters,
                          time_between_updates_ms);
  location_requests_.insert(
      LocationRequestMap::value_type(extension_id, location_request));
}

void LocationManager::RemoveLocationRequest(const std::string& extension_id,
                                            const std::string& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::pair<LocationRequestMap::iterator, LocationRequestMap::iterator>
      extension_range = location_requests_.equal_range(extension_id);

  for (LocationRequestMap::iterator it = extension_range.first;
       it != extension_range.second;
       ++it) {
    if (it->second->request_name() == name) {
      location_requests_.erase(it);
      return;
    }
  }
}

LocationManager::~LocationManager() {
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

void LocationManager::SendLocationUpdate(
    const std::string& extension_id,
    const std::string& request_name,
    const content::Geoposition& position) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_ptr<base::ListValue> args(new base::ListValue());
  std::string event_name;

  if (position.Validate() &&
      position.error_code == content::Geoposition::ERROR_CODE_NONE) {
    // Set data for onLocationUpdate event.
    location::Location location;
    location.name = request_name;
    GeopositionToApiCoordinates(position, &location.coords);
    location.timestamp = position.timestamp.ToJsTime();

    args->Append(location.ToValue().release());
    event_name = location::OnLocationUpdate::kEventName;
  } else {
    // Set data for onLocationError event.
    // TODO(vadimt): Set name.
    args->AppendString(position.error_message);
    event_name = location::OnLocationError::kEventName;
  }

  scoped_ptr<Event> event(new Event(event_name, args.Pass()));

  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(extension_id, event.Pass());
}

void LocationManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  // Grants permission to use geolocation once an extension with "location"
  // permission is loaded.
  if (extension->permissions_data()->HasAPIPermission(
          APIPermission::kLocation)) {
    content::GeolocationProvider::GetInstance()
        ->UserDidOptIntoLocationServices();
  }
}

void LocationManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  // Delete all requests from the unloaded extension.
  location_requests_.erase(extension->id());
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<LocationManager> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<LocationManager>*
LocationManager::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
LocationManager* LocationManager::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<LocationManager>::Get(context);
}

}  // namespace extensions
