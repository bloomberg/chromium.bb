// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_DATA_SOURCE_H_
#define COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_DATA_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace physical_web {

class PhysicalWebListener;

// Metadata struct for associating data with Physical Web URLs.
struct Metadata {
  Metadata();
  Metadata(const Metadata& other);
  ~Metadata();
  // The URL broadcasted by the beacon and scanned by the client device.
  // REQUIRED
  GURL scanned_url;
  // The URL that the scanned_url redirects to.
  // This is the URL that users should be directed to.
  // REQUIRED
  GURL resolved_url;
  // The favicon URL.
  // OPTIONAL
  GURL icon_url;
  // The title of the web page.
  // REQUIRED
  std::string title;
  // The description of the web page.
  // OPTIONAL: When the website has not specified a description, the PWS
  // generates one based on the initial text of the site, but this is not
  // guaranteed behavior.
  std::string description;
  // An identifier that associates multiple resolved URLs.  These URLs will
  // most typically be associated because their metadata is near-identical
  // (same icon, title, description, URL minus the fragment).  e.g.,
  // https://mymuseum/exhibits#e1
  // https://mymuseum/exhibits#e2
  // If two URLs have the same group id, only one should be shown (typically,
  // the one with the smallest distance estimate).
  // OPTIONAL: Treat the item as its own unique group if this is empty.
  std::string group_id;
  // The estimated distance between the user and the Physical Web device (e.g.,
  // beacon) in meters.
  // OPTIONAL: This will be a value <= 0 if no distance estimate has been
  // calculated.  The distance may not be calculated if we aren't able to
  // receive an estimate from the underlying scanning service in time, or if
  // (in the future) we begin sourcing Physical Web URLs from a non-BLE
  // transport (e.g. mDNS).
  double distance_estimate;
  // The timestamp corresponding to when this URL was last scanned.
  // REQUIRED
  base::Time scan_timestamp;
};

using MetadataList = std::vector<Metadata>;

// Helper class for accessing Physical Web metadata and controlling the scanner.
class PhysicalWebDataSource {
 public:
  virtual ~PhysicalWebDataSource() {}

  // Starts scanning for Physical Web URLs. If |network_request_enabled| is
  // true, discovered URLs will be sent to a resolution service.
  virtual void StartDiscovery(bool network_request_enabled) = 0;

  // Stops scanning for Physical Web URLs and clears cached URL content.
  virtual void StopDiscovery() = 0;

  // Returns a list of resolved URLs and associated page metadata. If network
  // requests are disabled or if discovery is not active, the list will be
  // empty. The method can be called at any time to receive the current metadata
  // list.
  virtual std::unique_ptr<MetadataList> GetMetadataList() = 0;

  // Returns boolean |true| if network requests are disabled and there are one
  // or more discovered URLs that have not been sent to the resolution service.
  // The method can be called at any time to check for unresolved discoveries.
  // If discovery is inactive or network requests are enabled, it will always
  // return false.
  virtual bool HasUnresolvedDiscoveries() = 0;

  // Register for changes to Physical Web URLs and associated page metadata.
  virtual void RegisterListener(PhysicalWebListener* physical_web_listener) = 0;

  // Unregister for changes to Physical Web URLs and associated page metadata.
  virtual void UnregisterListener(
      PhysicalWebListener* physical_web_listener) = 0;
};

}  // namespace physical_web

#endif  // COMPONENTS_PHYSICAL_WEB_DATA_SOURCE_PHYSICAL_WEB_DATA_SOURCE_H_
