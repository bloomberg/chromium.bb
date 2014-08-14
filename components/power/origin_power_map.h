// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_ORIGIN_POWER_MAP_H_
#define COMPONENTS_POWER_ORIGIN_POWER_MAP_H_

#include <map>

#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace power {

// Tracks app and website origins and how much power they are consuming while
// running.
class OriginPowerMap : public KeyedService {
 public:
  typedef std::map<GURL, int> PercentOriginMap;

  OriginPowerMap();
  virtual ~OriginPowerMap();

  // Returns the integer percentage usage of the total power consumed by a
  // given URL's origin.
  int GetPowerForOrigin(const GURL& url);

  // Adds a certain amount of power consumption to a given URL's origin.
  // |power| is a platform-specific heuristic estimating power consumption.
  void AddPowerForOrigin(const GURL& url, double power);

  // Returns a map of all origins to the integer percentage usage of power
  // consumed.
  PercentOriginMap GetPercentOriginMap();

 private:
  // OriginMap maps a URL to the amount of power consumed by the URL using the
  // same units as |total_consumed_|.
  typedef std::map<GURL, double> OriginMap;
  OriginMap origin_map_;

  // Total amount of power consumed using units determined by
  // the power heuristics available to the platform.
  double total_consumed_;

  DISALLOW_COPY_AND_ASSIGN(OriginPowerMap);
};

}  // namespace power

#endif  // COMPONENTS_POWER_ORIGIN_POWER_MAP_H_
