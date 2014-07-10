// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_impl.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace net {

////////////////////////////////////////////////////////////////////////////////
// HttpServerPropertiesManager

// The manager for creating and updating an HttpServerProperties (for example it
// tracks if a server supports SPDY or not).
//
// This class interacts with both the pref thread, where notifications of pref
// changes are received from, and the network thread, which owns it, and it
// persists the changes from network stack whether server supports SPDY or not.
//
// It must be constructed on the pref thread, to set up |pref_task_runner_| and
// the prefs listeners.
//
// ShutdownOnPrefThread must be called from pref thread before destruction, to
// release the prefs listeners on the pref thread.
//
// Class requires that update tasks from the Pref thread can post safely to the
// network thread, so the destruction order must guarantee that if |this|
// exists in pref thread, then a potential destruction on network thread will
// come after any task posted to network thread from that method on pref thread.
// This is used to go through network thread before the actual update starts,
// and grab a WeakPtr.
class NET_EXPORT HttpServerPropertiesManager : public HttpServerProperties {
 public:
  // Create an instance of the HttpServerPropertiesManager. The lifetime of the
  // PrefService objects must be longer than that of the
  // HttpServerPropertiesManager object. Must be constructed on the Pref thread.
  HttpServerPropertiesManager(
      PrefService* pref_service,
      const char* pref_path,
      scoped_refptr<base::SequencedTaskRunner> network_task_runner);
  virtual ~HttpServerPropertiesManager();

  // Initialize on Network thread.
  void InitializeOnNetworkThread();

  // Prepare for shutdown. Must be called on the Pref thread before destruction.
  void ShutdownOnPrefThread();

  // Helper function for unit tests to set the version in the dictionary.
  static void SetVersion(base::DictionaryValue* http_server_properties_dict,
                         int version_number);

  // Deletes all data. Works asynchronously, but if a |completion| callback is
  // provided, it will be fired on the pref thread when everything is done.
  void Clear(const base::Closure& completion);

  // ----------------------------------
  // HttpServerProperties methods:
  // ----------------------------------

  // Gets a weak pointer for this object.
  virtual base::WeakPtr<HttpServerProperties> GetWeakPtr() OVERRIDE;

  // Deletes all data. Works asynchronously.
  virtual void Clear() OVERRIDE;

  // Returns true if |server| supports SPDY. Should only be called from IO
  // thread.
  virtual bool SupportsSpdy(const HostPortPair& server) OVERRIDE;

  // Add |server| as the SPDY server which supports SPDY protocol into the
  // persisitent store. Should only be called from IO thread.
  virtual void SetSupportsSpdy(const HostPortPair& server,
                               bool support_spdy) OVERRIDE;

  // Returns true if |server| has an Alternate-Protocol header.
  virtual bool HasAlternateProtocol(const HostPortPair& server) OVERRIDE;

  // Returns the Alternate-Protocol and port for |server|.
  // HasAlternateProtocol(server) must be true.
  virtual AlternateProtocolInfo GetAlternateProtocol(
      const HostPortPair& server) OVERRIDE;

  // Sets the Alternate-Protocol for |server|.
  virtual void SetAlternateProtocol(
      const HostPortPair& server,
      uint16 alternate_port,
      AlternateProtocol alternate_protocol,
      double alternate_probability) OVERRIDE;

  // Sets the Alternate-Protocol for |server| to be BROKEN.
  virtual void SetBrokenAlternateProtocol(const HostPortPair& server) OVERRIDE;

  // Returns true if Alternate-Protocol for |server| was recently BROKEN.
  virtual bool WasAlternateProtocolRecentlyBroken(
      const HostPortPair& server) OVERRIDE;

  // Confirms that Alternate-Protocol for |server| is working.
  virtual void ConfirmAlternateProtocol(const HostPortPair& server) OVERRIDE;

  // Clears the Alternate-Protocol for |server|.
  virtual void ClearAlternateProtocol(const HostPortPair& server) OVERRIDE;

  // Returns all Alternate-Protocol mappings.
  virtual const AlternateProtocolMap& alternate_protocol_map() const OVERRIDE;

  virtual void SetAlternateProtocolExperiment(
      AlternateProtocolExperiment experiment) OVERRIDE;

  virtual void SetAlternateProtocolProbabilityThreshold(
      double threshold) OVERRIDE;

  virtual AlternateProtocolExperiment GetAlternateProtocolExperiment()
      const OVERRIDE;

  // Gets a reference to the SettingsMap stored for a host.
  // If no settings are stored, returns an empty SettingsMap.
  virtual const SettingsMap& GetSpdySettings(
      const HostPortPair& host_port_pair) OVERRIDE;

  // Saves an individual SPDY setting for a host. Returns true if SPDY setting
  // is to be persisted.
  virtual bool SetSpdySetting(const HostPortPair& host_port_pair,
                              SpdySettingsIds id,
                              SpdySettingsFlags flags,
                              uint32 value) OVERRIDE;

  // Clears all SPDY settings for a host.
  virtual void ClearSpdySettings(const HostPortPair& host_port_pair) OVERRIDE;

  // Clears all SPDY settings for all hosts.
  virtual void ClearAllSpdySettings() OVERRIDE;

  // Returns all SPDY persistent settings.
  virtual const SpdySettingsMap& spdy_settings_map() const OVERRIDE;

  virtual void SetServerNetworkStats(const HostPortPair& host_port_pair,
                                     NetworkStats stats) OVERRIDE;

  virtual const NetworkStats* GetServerNetworkStats(
      const HostPortPair& host_port_pair) const OVERRIDE;

 protected:
  // --------------------
  // SPDY related methods

  // These are used to delay updating of the cached data in
  // |http_server_properties_impl_| while the preferences are changing, and
  // execute only one update per simultaneous prefs changes.
  void ScheduleUpdateCacheOnPrefThread();

  // Starts the timers to update the cached prefs. This are overridden in tests
  // to prevent the delay.
  virtual void StartCacheUpdateTimerOnPrefThread(base::TimeDelta delay);

  // Update cached prefs in |http_server_properties_impl_| with data from
  // preferences. It gets the data on pref thread and calls
  // UpdateSpdyServersFromPrefsOnNetworkThread() to perform the update on
  // network thread.
  virtual void UpdateCacheFromPrefsOnPrefThread();

  // Starts the update of cached prefs in |http_server_properties_impl_| on the
  // network thread. Protected for testing.
  void UpdateCacheFromPrefsOnNetworkThread(
      std::vector<std::string>* spdy_servers,
      SpdySettingsMap* spdy_settings_map,
      AlternateProtocolMap* alternate_protocol_map,
      AlternateProtocolExperiment alternate_protocol_experiment,
      bool detected_corrupted_prefs);

  // These are used to delay updating the preferences when cached data in
  // |http_server_properties_impl_| is changing, and execute only one update per
  // simultaneous spdy_servers or spdy_settings or alternate_protocol changes.
  void ScheduleUpdatePrefsOnNetworkThread();

  // Starts the timers to update the prefs from cache. This are overridden in
  // tests to prevent the delay.
  virtual void StartPrefsUpdateTimerOnNetworkThread(base::TimeDelta delay);

  // Update prefs::kHttpServerProperties in preferences with the cached data
  // from |http_server_properties_impl_|. This gets the data on network thread
  // and posts a task (UpdatePrefsOnPrefThread) to update preferences on pref
  // thread.
  void UpdatePrefsFromCacheOnNetworkThread();

  // Same as above, but fires an optional |completion| callback on pref thread
  // when finished. Virtual for testing.
  virtual void UpdatePrefsFromCacheOnNetworkThread(
      const base::Closure& completion);

  // Update prefs::kHttpServerProperties preferences on pref thread. Executes an
  // optional |completion| callback when finished. Protected for testing.
  void UpdatePrefsOnPrefThread(base::ListValue* spdy_server_list,
                               SpdySettingsMap* spdy_settings_map,
                               AlternateProtocolMap* alternate_protocol_map,
                               const base::Closure& completion);

 private:
  void OnHttpServerPropertiesChanged();

  // -----------
  // Pref thread
  // -----------

  const scoped_refptr<base::SequencedTaskRunner> pref_task_runner_;

  // Used to get |weak_ptr_| to self on the pref thread.
  scoped_ptr<base::WeakPtrFactory<HttpServerPropertiesManager> >
      pref_weak_ptr_factory_;

  base::WeakPtr<HttpServerPropertiesManager> pref_weak_ptr_;

  // Used to post cache update tasks.
  scoped_ptr<base::OneShotTimer<HttpServerPropertiesManager> >
      pref_cache_update_timer_;

  // Used to track the spdy servers changes.
  PrefChangeRegistrar pref_change_registrar_;
  PrefService* pref_service_;  // Weak.
  bool setting_prefs_;
  const char* path_;

  // --------------
  // Network thread
  // --------------

  const scoped_refptr<base::SequencedTaskRunner> network_task_runner_;

  // Used to get |weak_ptr_| to self on the network thread.
  scoped_ptr<base::WeakPtrFactory<HttpServerPropertiesManager> >
      network_weak_ptr_factory_;

  // Used to post |prefs::kHttpServerProperties| pref update tasks.
  scoped_ptr<base::OneShotTimer<HttpServerPropertiesManager> >
      network_prefs_update_timer_;

  scoped_ptr<HttpServerPropertiesImpl> http_server_properties_impl_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManager);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_
