// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAST_CONFIG_DELEGATE_H_
#define ASH_CAST_CONFIG_DELEGATE_H_

#include <map>
#include <string>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace ash {

// This delegate allows the UI code in ash, e.g. |TrayCastDetailedView|,
// to access the cast extension.
class CastConfigDelegate {
 public:
  struct ASH_EXPORT Receiver {
    Receiver();
    ~Receiver();

    std::string id;
    base::string16 name;
  };

  struct ASH_EXPORT Activity {
    // The tab identifier that we are casting. These are the special tab values
    // taken from the chromecast extension itself. If an actual tab is being
    // casted, then the TabId will be >= 0.
    enum TabId {
      EXTENSION = -1,
      DESKTOP = -2,
      DISCOVERED_ACTIVITY = -3,
      EXTERNAL_EXTENSION_CLIENT = -4
    };

    Activity();
    ~Activity();

    std::string id;
    base::string16 title;
    std::string activity_type;
    bool allow_stop = false;

    // The id for the tab we are casting. Could be one of the TabId values,
    // or a value >= 0 that represents that tab index of the tab we are
    // casting. We default to casting the desktop, as a tab may not
    // necessarily exist.
    int tab_id = TabId::DESKTOP;
  };

  struct ASH_EXPORT ReceiverAndActivity {
    ReceiverAndActivity();
    ~ReceiverAndActivity();

    Receiver receiver;
    Activity activity;
  };

  // The key is the receiver id.
  using ReceiversAndActivites = std::map<std::string, ReceiverAndActivity>;
  using ReceiversAndActivitesCallback =
      base::Callback<void(const ReceiversAndActivites&)>;

  virtual ~CastConfigDelegate() {}

  // Returns true if cast extension is installed.
  virtual bool HasCastExtension() const = 0;

  // Fetches the current set of receivers and their possible activities. This
  // method will lookup the current chromecast extension and query its current
  // state. The |callback| will be invoked when the receiver/activity data is
  // available.
  virtual void GetReceiversAndActivities(
      const ReceiversAndActivitesCallback& callback) = 0;

  // Cast to a receiver specified by |receiver_id|.
  virtual void CastToReceiver(const std::string& receiver_id) = 0;

  // Stop ongoing cast. The |activity_id| is the unique identifier associated
  // with the ongoing cast. Each receiver has only one possible activity
  // associated with it. The |activity_id| is available by invoking
  // GetReceiversAndActivities(); if the receiver is currently casting, then the
  // associated activity data will have an id. This id can be used to stop the
  // cast in this method.
  virtual void StopCasting(const std::string& activity_id) = 0;

  // Opens Options page for cast.
  virtual void LaunchCastOptions() = 0;

 private:
  DISALLOW_ASSIGN(CastConfigDelegate);
};

}  // namespace ash

#endif  // ASH_CAST_CONFIG_DELEGATE_H_
