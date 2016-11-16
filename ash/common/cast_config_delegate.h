// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_CAST_CONFIG_DELEGATE_H_
#define ASH_COMMON_CAST_CONFIG_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace ash {

// This delegate allows the UI code in ash, e.g. |TrayCastDetailedView|,
// to access the cast extension.
class CastConfigDelegate {
 public:
  struct ASH_EXPORT Sink {
    Sink();
    ~Sink();

    std::string id;
    base::string16 name;
    base::string16 domain;
  };

  struct ASH_EXPORT Route {
    enum class ContentSource { UNKNOWN, TAB, DESKTOP };

    Route();
    ~Route();

    std::string id;
    base::string16 title;

    // Is the activity originating from this computer?
    bool is_local_source = false;

    // What is source of the content? For example, we could be DIAL casting a
    // tab or mirroring the entire desktop.
    ContentSource content_source = ContentSource::UNKNOWN;
  };

  struct ASH_EXPORT SinkAndRoute {
    SinkAndRoute();
    ~SinkAndRoute();

    Sink sink;
    Route route;
  };

  using SinksAndRoutes = std::vector<SinkAndRoute>;

  class ASH_EXPORT Observer {
   public:
    // Invoked whenever there is new sink or route information available.
    virtual void OnDevicesUpdated(const SinksAndRoutes& devices) = 0;

   protected:
    virtual ~Observer() {}

   private:
    DISALLOW_ASSIGN(Observer);
  };

  virtual ~CastConfigDelegate() {}

  // Request fresh data from the backend. When the data is available, all
  // registered observers will get called.
  virtual void RequestDeviceRefresh() = 0;

  // Initiate a casting session to |sink|.
  virtual void CastToSink(const Sink& sink) = 0;

  // A user-initiated request to stop the given cast session.
  virtual void StopCasting(const Route& route) = 0;

  // Add or remove an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 private:
  DISALLOW_ASSIGN(CastConfigDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_CAST_CONFIG_DELEGATE_H_
