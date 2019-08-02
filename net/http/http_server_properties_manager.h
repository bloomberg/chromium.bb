// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/http_server_properties.h"
#include "net/log/net_log_with_source.h"

namespace base {
class DictionaryValue;
class TickClock;
}

namespace net {

class IPAddress;

////////////////////////////////////////////////////////////////////////////////
// HttpServerPropertiesManager

// Class responsible for serializing/deserializing HttpServerProperties and
// reading from/writing to preferences.
class NET_EXPORT HttpServerPropertiesManager {
 public:
  // Provides an interface to interact with persistent preferences storage
  // implemented by the embedder. The prefs are assumed not to have been loaded
  // before HttpServerPropertiesManager construction.
  class NET_EXPORT PrefDelegate {
   public:
    virtual ~PrefDelegate();

    // Returns the branch of the preferences system for the server properties.
    // Returns nullptr if the pref system has no data for the server properties.
    virtual const base::DictionaryValue* GetServerProperties() const = 0;

    // Sets the server properties to the given value. If |callback| is
    // non-empty, flushes data to persistent storage and invokes |callback|
    // asynchronously when complete.
    virtual void SetServerProperties(const base::DictionaryValue& value,
                                     base::OnceClosure callback) = 0;

    // Starts listening for prefs to be loaded. If prefs are already loaded,
    // |pref_loaded_callback| will be invoked asynchronously. Callback will be
    // invoked even if prefs fail to load. Will only be called once by the
    // HttpServerPropertiesManager.
    virtual void WaitForPrefLoad(base::OnceClosure pref_loaded_callback) = 0;
  };

  // Called when prefs are loaded. If prefs completely failed to load,
  // everything will be nullptr. Otherwise, everything will be populated, except
  // |broken_alternative_service_list| and
  // |recently_broken_alternative_services|, which may be null. If prefs were
  // present but some were corrupt, |prefs_corrupt| will be true.
  using OnPrefsLoadedCallback = base::OnceCallback<void(
      std::unique_ptr<SpdyServersMap> spdy_servers_map,
      std::unique_ptr<AlternativeServiceMap> alternative_service_map,
      std::unique_ptr<ServerNetworkStatsMap> server_network_stats_map,
      const IPAddress& last_quic_address,
      std::unique_ptr<QuicServerInfoMap> quic_server_info_map,
      std::unique_ptr<BrokenAlternativeServiceList>
          broken_alternative_service_list,
      std::unique_ptr<RecentlyBrokenAlternativeServices>
          recently_broken_alternative_services,
      bool prefs_corrupt)>;

  using GetCannonicalSuffix =
      base::RepeatingCallback<const std::string*(const std::string& host)>;

  // Create an instance of the HttpServerPropertiesManager.
  //
  // |on_prefs_loaded_callback| will be invoked with values loaded from
  // |prefs_delegate| once prefs have been loaded from disk.
  // If WriteToPrefs() is invoked before this happens,
  // |on_prefs_loaded_callback| will never be invoked, since the written prefs
  // take precedence over the ones previously stored on disk.
  //
  // |clock| is used for setting expiration times and scheduling the
  // expiration of broken alternative services, and must not be nullptr.
  HttpServerPropertiesManager(std::unique_ptr<PrefDelegate> pref_delegate,
                              OnPrefsLoadedCallback on_prefs_loaded_callback,
                              size_t max_server_configs_stored_in_properties,
                              NetLog* net_log,
                              const base::TickClock* clock = nullptr);

  ~HttpServerPropertiesManager();

  // Populates passed in objects with data from preferences. If pref data is not
  // present, leaves all values alone. Otherwise, populates them all, with the
  // possible exception of the two broken alt services lists.
  //
  // TODO(mmenke): Consider always populating fields, unconditionally, for a
  // simpler API.
  void ReadPrefs(
      std::unique_ptr<SpdyServersMap>* spdy_servers_map,
      std::unique_ptr<AlternativeServiceMap>* alternative_service_map,
      std::unique_ptr<ServerNetworkStatsMap>* server_network_stats_map,
      IPAddress* last_quic_address,
      std::unique_ptr<QuicServerInfoMap>* quic_server_info_map,
      std::unique_ptr<BrokenAlternativeServiceList>*
          broken_alternative_service_list,
      std::unique_ptr<RecentlyBrokenAlternativeServices>*
          recently_broken_alternative_services,
      bool* detected_corrupted_prefs);

  void set_max_server_configs_stored_in_properties(
      size_t max_server_configs_stored_in_properties) {
    max_server_configs_stored_in_properties_ =
        max_server_configs_stored_in_properties;
  }

  // Update preferences with caller-provided data. Invokes |callback| when
  // changes have been committed, if non-null.
  //
  // If the OnPrefLoadCallback() passed to the constructor hasn't been invoked
  // by the time this method is called, calling this will prevent it from ever
  // being invoked, as this method will overwrite any previous preferences.
  void WriteToPrefs(
      const SpdyServersMap& spdy_servers_map,
      const AlternativeServiceMap& alternative_service_map,
      const GetCannonicalSuffix& get_canonical_suffix,
      const ServerNetworkStatsMap& server_network_stats_map,
      const IPAddress& last_quic_address,
      const QuicServerInfoMap& quic_server_info_map,
      const BrokenAlternativeServiceList& broken_alternative_service_list,
      const RecentlyBrokenAlternativeServices&
          recently_broken_alternative_services,
      base::OnceClosure callback);

 private:
  // TODO(mmenke): Remove these friend methods, and make all methods static that
  // can be.
  FRIEND_TEST_ALL_PREFIXES(HttpServerPropertiesManagerTest,
                           AddToAlternativeServiceMap);
  FRIEND_TEST_ALL_PREFIXES(HttpServerPropertiesManagerTest,
                           ReadAdvertisedVersionsFromPref);
  FRIEND_TEST_ALL_PREFIXES(HttpServerPropertiesManagerTest,
                           DoNotLoadAltSvcForInsecureOrigins);
  FRIEND_TEST_ALL_PREFIXES(HttpServerPropertiesManagerTest,
                           DoNotLoadExpiredAlternativeService);
  bool AddServersData(const base::DictionaryValue& server_dict,
                      SpdyServersMap* spdy_servers_map,
                      AlternativeServiceMap* alternative_service_map,
                      ServerNetworkStatsMap* network_stats_map);
  // Helper method used for parsing an alternative service from JSON.
  // |dict| is the JSON dictionary to be parsed. It should contain fields
  // corresponding to members of AlternativeService.
  // |host_optional| determines whether or not the "host" field is optional. If
  // optional, the default value is empty string.
  // |parsing_under| is used only for debug log outputs in case of error; it
  // should describe what section of the JSON prefs is currently being parsed.
  // |alternative_service| is the output of parsing |dict|.
  // Return value is true if parsing is successful.
  bool ParseAlternativeServiceDict(const base::DictionaryValue& dict,
                                   bool host_optional,
                                   const std::string& parsing_under,
                                   AlternativeService* alternative_service);
  bool ParseAlternativeServiceInfoDictOfServer(
      const base::DictionaryValue& dict,
      const std::string& server_str,
      AlternativeServiceInfo* alternative_service_info);
  bool AddToAlternativeServiceMap(
      const url::SchemeHostPort& server,
      const base::DictionaryValue& server_dict,
      AlternativeServiceMap* alternative_service_map);
  bool ReadSupportsQuic(const base::DictionaryValue& server_dict,
                        IPAddress* last_quic_address);
  bool AddToNetworkStatsMap(const url::SchemeHostPort& server,
                            const base::DictionaryValue& server_dict,
                            ServerNetworkStatsMap* network_stats_map);
  bool AddToQuicServerInfoMap(const base::DictionaryValue& server_dict,
                              QuicServerInfoMap* quic_server_info_map);
  bool AddToBrokenAlternativeServices(
      const base::DictionaryValue& broken_alt_svc_entry_dict,
      BrokenAlternativeServiceList* broken_alternative_service_list,
      RecentlyBrokenAlternativeServices* recently_broken_alternative_services);

  void SaveAlternativeServiceToServerPrefs(
      const AlternativeServiceInfoVector& alternative_service_info_vector,
      base::DictionaryValue* server_pref_dict);
  void SaveSupportsQuicToPrefs(
      const IPAddress& last_quic_address,
      base::DictionaryValue* http_server_properties_dict);
  void SaveNetworkStatsToServerPrefs(
      const ServerNetworkStats& server_network_stats,
      base::DictionaryValue* server_pref_dict);
  void SaveQuicServerInfoMapToServerPrefs(
      const QuicServerInfoMap& quic_server_info_map,
      base::DictionaryValue* http_server_properties_dict);
  void SaveBrokenAlternativeServicesToPrefs(
      const BrokenAlternativeServiceList& broken_alternative_service_list,
      size_t max_broken_alternative_services,
      const RecentlyBrokenAlternativeServices&
          recently_broken_alternative_services,
      base::DictionaryValue* http_server_properties_dict);

  void OnHttpServerPropertiesLoaded();

  std::unique_ptr<PrefDelegate> pref_delegate_;

  OnPrefsLoadedCallback on_prefs_loaded_callback_;

  size_t max_server_configs_stored_in_properties_;

  const base::TickClock* clock_;  // Unowned

  const NetLogWithSource net_log_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HttpServerPropertiesManager> pref_load_weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManager);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_
