// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAST_CONFIG_DELEGATE_H_
#define ASH_CAST_CONFIG_DELEGATE_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/callback_list.h"
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
      EXTERNAL_EXTENSION_CLIENT = -4,

      // Not in the extension. Used when the extension does not give us a tabId
      // (ie, the cast is running from another device).
      UNKNOWN = -5
    };

    Activity();
    ~Activity();

    std::string id;
    base::string16 title;

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
  using ReceiversAndActivites = std::vector<ReceiverAndActivity>;
  using ReceiversAndActivitesCallback =
      base::Callback<void(const ReceiversAndActivites&)>;
  using DeviceUpdateSubscription = scoped_ptr<
      base::CallbackList<void(const ReceiversAndActivites&)>::Subscription>;

  virtual ~CastConfigDelegate() {}

  // Returns true if cast extension is installed.
  virtual bool HasCastExtension() const = 0;

  // Adds a listener that will get invoked whenever the receivers or their
  // associated activites have changed.
  virtual DeviceUpdateSubscription RegisterDeviceUpdateObserver(
      const ReceiversAndActivitesCallback& callback) = 0;

  // Request fresh data from the backend. When the data is available, all
  // registered observers will get called.
  virtual void RequestDeviceRefresh() = 0;

  // Cast to a receiver specified by |receiver_id|.
  virtual void CastToReceiver(const std::string& receiver_id) = 0;

  // Stop an ongoing cast (this should be a user initiated stop).
  virtual void StopCasting() = 0;

  // Opens Options page for cast.
  virtual void LaunchCastOptions() = 0;

 private:
  DISALLOW_ASSIGN(CastConfigDelegate);
};

}  // namespace ash

#endif  // ASH_CAST_CONFIG_DELEGATE_H_
