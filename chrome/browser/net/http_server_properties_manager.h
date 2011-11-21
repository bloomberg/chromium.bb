// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_H_
#define CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_H_
#pragma once

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_impl.h"

class PrefService;

namespace chrome_browser_net {

////////////////////////////////////////////////////////////////////////////////
//  HttpServerPropertiesManager

// The manager for creating and updating an HttpServerProperties (for example it
// tracks if a server supports SPDY or not).
//
// This class interacts with both the UI thread, where notifications of pref
// changes are received from, and the IO thread, which owns it (in the
// ProfileIOData) and it persists the changes from network stack that if a
// server supports SPDY or not.
//
// It must be constructed on the UI thread, to set up |ui_method_factory_| and
// the prefs listeners.
//
// ShutdownOnUIThread must be called from UI before destruction, to release
// the prefs listeners on the UI thread. This is done from ProfileIOData.
//
// Update tasks from the UI thread can post safely to the IO thread, since the
// destruction order of Profile and ProfileIOData guarantees that if this
// exists in UI, then a potential destruction on IO will come after any task
// posted to IO from that method on UI. This is used to go through IO before
// the actual update starts, and grab a WeakPtr.
class HttpServerPropertiesManager
    : public net::HttpServerProperties,
      public content::NotificationObserver {
 public:
  // Create an instance of the HttpServerPropertiesManager. The lifetime of the
  // PrefService objects must be longer than that of the
  // HttpServerPropertiesManager object. Must be constructed on the UI thread.
  explicit HttpServerPropertiesManager(PrefService* pref_service);
  virtual ~HttpServerPropertiesManager();

  // Initialize |http_server_properties_impl_| and |io_method_factory_| on IO
  // thread. It also posts a task to UI thread to get SPDY Server preferences
  // from |pref_service_|.
  void InitializeOnIOThread();

  // Prepare for shutdown. Must be called on the UI thread, before destruction.
  void ShutdownOnUIThread();

  // Register |prefs| for properties managed here.
  static void RegisterPrefs(PrefService* prefs);

  // ----------------------------------
  // net::HttpServerProperties methods:
  // ----------------------------------

  // Deletes all data.
  virtual void Clear() OVERRIDE;

  // Returns true if |server| supports SPDY. Should only be called from IO
  // thread.
  virtual bool SupportsSpdy(const net::HostPortPair& server) const OVERRIDE;

  // Add |server| as the SPDY server which supports SPDY protocol into the
  // persisitent store. Should only be called from IO thread.
  virtual void SetSupportsSpdy(const net::HostPortPair& server,
                               bool support_spdy) OVERRIDE;

  // Returns true if |server| has an Alternate-Protocol header.
  virtual bool HasAlternateProtocol(
      const net::HostPortPair& server) const OVERRIDE;

  // Returns the Alternate-Protocol and port for |server|.
  // HasAlternateProtocol(server) must be true.
  virtual net::PortAlternateProtocolPair GetAlternateProtocol(
      const net::HostPortPair& server) const OVERRIDE;

  // Sets the Alternate-Protocol for |server|.
  virtual void SetAlternateProtocol(
      const net::HostPortPair& server,
      uint16 alternate_port,
      net::AlternateProtocol alternate_protocol) OVERRIDE;

  // Sets the Alternate-Protocol for |server| to be BROKEN.
  virtual void SetBrokenAlternateProtocol(
      const net::HostPortPair& server) OVERRIDE;

  // Returns all Alternate-Protocol mappings.
  virtual const net::AlternateProtocolMap&
      alternate_protocol_map() const OVERRIDE;

  // Gets a reference to the SpdySettings stored for a host.
  // If no settings are stored, returns an empty set of settings.
  virtual const spdy::SpdySettings& GetSpdySettings(
      const net::HostPortPair& host_port_pair) const OVERRIDE;

  // Saves settings for a host. Returns true if SpdySettings are persisted.
  virtual bool SetSpdySettings(
      const net::HostPortPair& host_port_pair,
      const spdy::SpdySettings& settings) OVERRIDE;

  // Clears all spdy_settings.
  virtual void ClearSpdySettings() OVERRIDE;

  // Returns all SpdySettings mappings.
  virtual const net::SpdySettingsMap& spdy_settings_map() const OVERRIDE;

 protected:
  // --------------------
  // SPDY related methods

  // These are used to delay updating of the cached data in
  // |http_server_properties_impl_| while the preferences are changing, and
  // execute only one update per simultaneous prefs changes.
  void ScheduleUpdateCacheOnUI();

  // Starts the timers to update the cached prefs. This are overridden in tests
  // to prevent the delay.
  virtual void StartCacheUpdateTimerOnUI(base::TimeDelta delay);

  // Update cached prefs in |http_server_properties_impl_| with data from
  // preferences. It gets the data on UI thread and calls
  // UpdateSpdyServersFromPrefsOnIO() to perform the update on IO thread.
  virtual void UpdateCacheFromPrefsOnUI();

  // Starts the update of cached prefs in |http_server_properties_impl_| on the
  // IO thread. Protected for testing.
  void UpdateCacheFromPrefsOnIO(
      std::vector<std::string>* spdy_servers,
      net::SpdySettingsMap* spdy_settings_map,
      net::AlternateProtocolMap* alternate_protocol_map);

  // These are used to delay updating the preferences when cached data in
  // |http_server_properties_impl_| is changing, and execute only one update per
  // simultaneous spdy_servers or spdy_settings or alternate_protocol changes.
  void ScheduleUpdatePrefsOnIO();

  // Starts the timers to update the prefs from cache. This are overridden in
  // tests to prevent the delay.
  virtual void StartPrefsUpdateTimerOnIO(base::TimeDelta delay);

  // Update prefs::kHttpServerProperties in preferences with the cached data
  // from |http_server_properties_impl_|. This gets the data on IO thread and
  // posts a task (UpdatePrefsOnUI) to update the preferences UI thread.
  // Virtual for testing.
  virtual void UpdatePrefsFromCacheOnIO();

  // Update prefs::kHttpServerProperties preferences on UI thread. Protected for
  // testing.
  void UpdatePrefsOnUI(
      base::ListValue* spdy_server_list,
      net::SpdySettingsMap* spdy_settings_map,
      net::AlternateProtocolMap* alternate_protocol_map);

 private:
  // Callback for preference changes.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ---------
  // UI thread
  // ---------

  // Used to get |weak_ptr_| to self on the UI thread.
  scoped_ptr<base::WeakPtrFactory<HttpServerPropertiesManager> >
      ui_weak_ptr_factory_;

  base::WeakPtr<HttpServerPropertiesManager> ui_weak_ptr_;

  // Used to post cache update tasks.
  scoped_ptr<base::OneShotTimer<HttpServerPropertiesManager> >
      ui_cache_update_timer_;

  // Used to track the spdy servers changes.
  PrefChangeRegistrar pref_change_registrar_;
  PrefService* pref_service_;  // Weak.
  bool setting_prefs_;

  // ---------
  // IO thread
  // ---------

  // Used to post |prefs::kHttpServerProperties| pref update tasks.
  scoped_ptr<base::OneShotTimer<HttpServerPropertiesManager> >
      io_prefs_update_timer_;

  scoped_ptr<net::HttpServerPropertiesImpl> http_server_properties_impl_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManager);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_HTTP_SERVER_PROPERTIES_MANAGER_H_
