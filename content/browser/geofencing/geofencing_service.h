// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOFENCING_GEOFENCING_SERVICE_H_
#define CONTENT_BROWSER_GEOFENCING_GEOFENCING_SERVICE_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/geofencing_types.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace blink {
struct WebCircularGeofencingRegion;
};

namespace content {

class GeofencingProvider;
class GeofencingRegistrationDelegate;

// This interface exists primarily to facilitate testing of classes that depend
// on GeofencingService. It defines the interface exposed by
// |GeofencingService|.
class GeofencingService {
 public:
  virtual ~GeofencingService() {}

  // Returns if a geofencing service is available.
  virtual bool IsServiceAvailable() = 0;

  // Register a region. This returns a unique registration ID, and
  // asynchronously calls the |delegate|s RegistrationFinished method to
  // inform the delegate of the result of the attempt to register the region.
  // This does not transfer ownership of the |delegate|. Callers have to ensure
  // that the delegate remains valid as long as the region is registered.
  virtual int64_t RegisterRegion(
      const blink::WebCircularGeofencingRegion& region,
      GeofencingRegistrationDelegate* delegate) = 0;

  // Unregister a region. This is assumed to always succeed. It is safe to call
  // this even if a registration is still in progress.
  virtual void UnregisterRegion(int64_t geofencing_registration_id) = 0;
};

// This class combines all the geofence registrations from the various per
// storage partition |GeofencingManager| instances, and registers a subset
// of those fences with an underlying platform specific |GeofencingProvider|.
// TODO(mek): Limit the number of geofences that are registered with the
// underlying GeofencingProvider.
class CONTENT_EXPORT GeofencingServiceImpl
    : NON_EXPORTED_BASE(public GeofencingService) {
 public:
  // Gets a pointer to the singleton instance of the geofencing service. This
  // must only be called on the IO thread so that the GeofencingService is
  // always instantiated on the same thread. Ownership is NOT returned.
  static GeofencingServiceImpl* GetInstance();

  // GeofencingServiceInterface implementation.
  bool IsServiceAvailable() override;
  int64_t RegisterRegion(const blink::WebCircularGeofencingRegion& region,
                         GeofencingRegistrationDelegate* delegate) override;
  void UnregisterRegion(int64_t geofencing_registration_id) override;

 protected:
  friend class GeofencingServiceTest;
  friend struct base::DefaultSingletonTraits<GeofencingServiceImpl>;
  GeofencingServiceImpl();
  ~GeofencingServiceImpl() override;

  void SetProviderForTesting(scoped_ptr<GeofencingProvider> provider);
  int RegistrationCountForTesting();

 private:
  struct Registration;
  typedef std::map<int64_t, Registration> RegistrationsMap;

  // This method checks if a |GeofencingProvider| exists, creates a new one if
  // not, and finally returns false if it can't create a provider for the
  // current platform.
  bool EnsureProvider();

  // Returns a new unique ID to use for the next geofence registration.
  int64_t GetNextId();

  // Notifies the correct delegate that registration has completed for a
  // specific geofence registration.
  void NotifyRegistrationFinished(int64_t geofencing_registration_id,
                                  GeofencingStatus status);

  int64_t next_registration_id_;
  RegistrationsMap registrations_;
  scoped_ptr<GeofencingProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(GeofencingServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOFENCING_GEOFENCING_SERVICE_H_
