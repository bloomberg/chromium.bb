// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/observer_list_threadsafe.h"
#include "base/time/time.h"
#include "net/base/net_export.h"

class GURL;

namespace net {

struct DnsConfig;
class HistogramWatcher;
class NetworkChangeNotifierFactory;
struct NetworkInterface;
typedef std::vector<NetworkInterface> NetworkInterfaceList;
class URLRequest;

#if defined(OS_LINUX)
namespace internal {
class AddressTrackerLinux;
}
#endif

// NetworkChangeNotifier monitors the system for network changes, and notifies
// registered observers of those events.  Observers may register on any thread,
// and will be called back on the thread from which they registered.
// NetworkChangeNotifiers are threadsafe, though they must be created and
// destroyed on the same thread.
class NET_EXPORT NetworkChangeNotifier {
 public:
  // This is a superset of the connection types in the NetInfo v3 specification:
  // http://w3c.github.io/netinfo/.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
  enum ConnectionType {
    CONNECTION_UNKNOWN = 0,  // A connection exists, but its type is unknown.
                             // Also used as a default value.
    CONNECTION_ETHERNET = 1,
    CONNECTION_WIFI = 2,
    CONNECTION_2G = 3,
    CONNECTION_3G = 4,
    CONNECTION_4G = 5,
    CONNECTION_NONE = 6,     // No connection.
    CONNECTION_BLUETOOTH = 7,
    CONNECTION_LAST = CONNECTION_BLUETOOTH
  };

  // This is the NetInfo v3 set of connection technologies as seen in
  // http://w3c.github.io/netinfo/. This enum is copied in
  // NetworkChangeNotifier.java so be sure to change both at once.
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
  enum ConnectionSubtype {
    SUBTYPE_GSM = 0,
    SUBTYPE_IDEN,
    SUBTYPE_CDMA,
    SUBTYPE_1XRTT,
    SUBTYPE_GPRS,
    SUBTYPE_EDGE,
    SUBTYPE_UMTS,
    SUBTYPE_EVDO_REV_0,
    SUBTYPE_EVDO_REV_A,
    SUBTYPE_HSPA,
    SUBTYPE_EVDO_REV_B,
    SUBTYPE_HSDPA,
    SUBTYPE_HSUPA,
    SUBTYPE_EHRPD,
    SUBTYPE_HSPAP,
    SUBTYPE_LTE,
    SUBTYPE_LTE_ADVANCED,
    SUBTYPE_BLUETOOTH_1_2,
    SUBTYPE_BLUETOOTH_2_1,
    SUBTYPE_BLUETOOTH_3_0,
    SUBTYPE_BLUETOOTH_4_0,
    SUBTYPE_ETHERNET,
    SUBTYPE_FAST_ETHERNET,
    SUBTYPE_GIGABIT_ETHERNET,
    SUBTYPE_10_GIGABIT_ETHERNET,
    SUBTYPE_WIFI_B,
    SUBTYPE_WIFI_G,
    SUBTYPE_WIFI_N,
    SUBTYPE_WIFI_AC,
    SUBTYPE_WIFI_AD,
    SUBTYPE_UNKNOWN,
    SUBTYPE_NONE,
    SUBTYPE_OTHER,
    SUBTYPE_LAST = SUBTYPE_OTHER
  };

  class NET_EXPORT IPAddressObserver {
   public:
    // Will be called when the IP address of the primary interface changes.
    // This includes when the primary interface itself changes.
    virtual void OnIPAddressChanged() = 0;

   protected:
    IPAddressObserver() {}
    virtual ~IPAddressObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(IPAddressObserver);
  };

  class NET_EXPORT ConnectionTypeObserver {
   public:
    // Will be called when the connection type of the system has changed.
    // See NetworkChangeNotifier::GetConnectionType() for important caveats
    // about the unreliability of using this signal to infer the ability to
    // reach remote sites.
    virtual void OnConnectionTypeChanged(ConnectionType type) = 0;

   protected:
    ConnectionTypeObserver() {}
    virtual ~ConnectionTypeObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ConnectionTypeObserver);
  };

  class NET_EXPORT DNSObserver {
   public:
    // Will be called when the DNS settings of the system may have changed.
    // Use GetDnsConfig to obtain the current settings.
    virtual void OnDNSChanged() = 0;

   protected:
    DNSObserver() {}
    virtual ~DNSObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(DNSObserver);
  };

  class NET_EXPORT NetworkChangeObserver {
   public:
    // OnNetworkChanged will be called when a change occurs to the host
    // computer's hardware or software that affects the route network packets
    // take to any network server. Some examples:
    //   1. A network connection becoming available or going away. For example
    //      plugging or unplugging an Ethernet cable, WiFi or cellular modem
    //      connecting or disconnecting from a network, or a VPN tunnel being
    //      established or taken down.
    //   2. An active network connection's IP address changes.
    //   3. A change to the local IP routing tables.
    // The signal shall only be produced when the change is complete.  For
    // example if a new network connection has become available, only give the
    // signal once we think the O/S has finished establishing the connection
    // (i.e. DHCP is done) to the point where the new connection is usable.
    // The signal shall not be produced spuriously as it will be triggering some
    // expensive operations, like socket pools closing all connections and
    // sockets and then re-establishing them.
    // |type| indicates the type of the active primary network connection after
    // the change.  Observers performing "constructive" activities like trying
    // to establish a connection to a server should only do so when
    // |type != CONNECTION_NONE|.  Observers performing "destructive" activities
    // like resetting already established server connections should only do so
    // when |type == CONNECTION_NONE|.  OnNetworkChanged will always be called
    // with CONNECTION_NONE immediately prior to being called with an online
    // state; this is done to make sure that destructive actions take place
    // prior to constructive actions.
    virtual void OnNetworkChanged(ConnectionType type) = 0;

   protected:
    NetworkChangeObserver() {}
    virtual ~NetworkChangeObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(NetworkChangeObserver);
  };

  class NET_EXPORT MaxBandwidthObserver {
   public:
    // Will be called when a change occurs to the network's maximum bandwidth as
    // defined in http://w3c.github.io/netinfo/. Generally this will only be
    // called on bandwidth changing network connection/disconnection events.
    // Some platforms may call it more frequently, such as when WiFi signal
    // strength changes.
    // TODO(jkarlin): This is currently only implemented for Android. Implement
    // on every platform.
    virtual void OnMaxBandwidthChanged(double max_bandwidth_mbps) = 0;

   protected:
    MaxBandwidthObserver() {}
    virtual ~MaxBandwidthObserver() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(MaxBandwidthObserver);
  };

  virtual ~NetworkChangeNotifier();

  // See the description of NetworkChangeNotifier::GetConnectionType().
  // Implementations must be thread-safe. Implementations must also be
  // cheap as it is called often.
  virtual ConnectionType GetCurrentConnectionType() const = 0;

  // Replaces the default class factory instance of NetworkChangeNotifier class.
  // The method will take over the ownership of |factory| object.
  static void SetFactory(NetworkChangeNotifierFactory* factory);

  // Creates the process-wide, platform-specific NetworkChangeNotifier.  The
  // caller owns the returned pointer.  You may call this on any thread.  You
  // may also avoid creating this entirely (in which case nothing will be
  // monitored), but if you do create it, you must do so before any other
  // threads try to access the API below, and it must outlive all other threads
  // which might try to use it.
  static NetworkChangeNotifier* Create();

  // Returns the connection type.
  // A return value of |CONNECTION_NONE| is a pretty strong indicator that the
  // user won't be able to connect to remote sites. However, another return
  // value doesn't imply that the user will be able to connect to remote sites;
  // even if some link is up, it is uncertain whether a particular connection
  // attempt to a particular remote site will be successful.
  // The returned value only describes the connection currently used by the
  // device, and does not take into account other machines on the network. For
  // example, if the device is connected using Wifi to a 3G gateway to access
  // the internet, the connection type is CONNECTION_WIFI.
  static ConnectionType GetConnectionType();

  // Returns a theoretical upper limit on download bandwidth, potentially based
  // on underlying connection type, signal strength, or some other signal. The
  // default mapping of connection type to maximum bandwidth is provided in the
  // NetInfo spec: http://w3c.github.io/netinfo/. Host-specific application
  // permissions may be required, please see host-specific declaration for more
  // information.
  static double GetMaxBandwidth();

  // Retrieve the last read DnsConfig. This could be expensive if the system has
  // a large HOSTS file.
  static void GetDnsConfig(DnsConfig* config);

#if defined(OS_LINUX)
  // Returns the AddressTrackerLinux if present.
  static const internal::AddressTrackerLinux* GetAddressTracker();
#endif

  // Convenience method to determine if the user is offline.
  // Returns true if there is currently no internet connection.
  //
  // A return value of |true| is a pretty strong indicator that the user
  // won't be able to connect to remote sites. However, a return value of
  // |false| is inconclusive; even if some link is up, it is uncertain
  // whether a particular connection attempt to a particular remote site
  // will be successfully.
  static bool IsOffline();

  // Returns true if |type| is a cellular connection.
  // Returns false if |type| is CONNECTION_UNKNOWN, and thus, depending on the
  // implementation of GetConnectionType(), it is possible that
  // IsConnectionCellular(GetConnectionType()) returns false even if the
  // current connection is cellular.
  static bool IsConnectionCellular(ConnectionType type);

  // Gets the current connection type based on |interfaces|. Returns
  // CONNECTION_NONE if there are no interfaces, CONNECTION_UNKNOWN if two
  // interfaces have different connection types or the connection type of all
  // interfaces if they have the same interface type.
  static ConnectionType ConnectionTypeFromInterfaceList(
      const NetworkInterfaceList& interfaces);

  // Like Create(), but for use in tests.  The mock object doesn't monitor any
  // events, it merely rebroadcasts notifications when requested.
  static NetworkChangeNotifier* CreateMock();

  // Registers |observer| to receive notifications of network changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.  This is safe to call if Create() has not
  // been called (as long as it doesn't race the Create() call on another
  // thread), in which case it will simply do nothing.
  static void AddIPAddressObserver(IPAddressObserver* observer);
  static void AddConnectionTypeObserver(ConnectionTypeObserver* observer);
  static void AddDNSObserver(DNSObserver* observer);
  static void AddNetworkChangeObserver(NetworkChangeObserver* observer);
  static void AddMaxBandwidthObserver(MaxBandwidthObserver* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.  Like AddObserver(),
  // this is safe to call if Create() has not been called (as long as it doesn't
  // race the Create() call on another thread), in which case it will simply do
  // nothing.  Technically, it's also safe to call after the notifier object has
  // been destroyed, if the call doesn't race the notifier's destruction, but
  // there's no reason to use the API in this risky way, so don't do it.
  static void RemoveIPAddressObserver(IPAddressObserver* observer);
  static void RemoveConnectionTypeObserver(ConnectionTypeObserver* observer);
  static void RemoveDNSObserver(DNSObserver* observer);
  static void RemoveNetworkChangeObserver(NetworkChangeObserver* observer);
  static void RemoveMaxBandwidthObserver(MaxBandwidthObserver* observer);

  // Allow unit tests to trigger notifications.
  static void NotifyObserversOfIPAddressChangeForTests();
  static void NotifyObserversOfConnectionTypeChangeForTests(
      ConnectionType type);
  static void NotifyObserversOfNetworkChangeForTests(ConnectionType type);

  // Enable or disable notifications from the host. After setting to true, be
  // sure to pump the RunLoop until idle to finish any preexisting
  // notifications.
  static void SetTestNotificationsOnly(bool test_only);

  // Return a string equivalent to |type|.
  static const char* ConnectionTypeToString(ConnectionType type);

  // Let the NetworkChangeNotifier know we received some data.
  // This is used for producing histogram data about the accuracy of
  // the NetworkChangenotifier's online detection and rough network
  // connection measurements.
  static void NotifyDataReceived(const URLRequest& request, int bytes_read);

  // Register the Observer callbacks for producing histogram data.  This
  // should be called from the network thread to avoid race conditions.
  // ShutdownHistogramWatcher() must be called prior to NetworkChangeNotifier
  // destruction.
  static void InitHistogramWatcher();

  // Unregister the Observer callbacks for producing histogram data.  This
  // should be called from the network thread to avoid race conditions.
  static void ShutdownHistogramWatcher();

  // Log the |NCN.NetworkOperatorMCCMNC| histogram.
  static void LogOperatorCodeHistogram(ConnectionType type);

  // Allows a second NetworkChangeNotifier to be created for unit testing, so
  // the test suite can create a MockNetworkChangeNotifier, but platform
  // specific NetworkChangeNotifiers can also be created for testing.  To use,
  // create an DisableForTest object, and then create the new
  // NetworkChangeNotifier object.  The NetworkChangeNotifier must be
  // destroyed before the DisableForTest object, as its destruction will restore
  // the original NetworkChangeNotifier.
  class NET_EXPORT DisableForTest {
   public:
    DisableForTest();
    ~DisableForTest();

   private:
    // The original NetworkChangeNotifier to be restored on destruction.
    NetworkChangeNotifier* network_change_notifier_;
  };

 protected:
  // NetworkChanged signal is calculated from the IPAddressChanged and
  // ConnectionTypeChanged signals. Delay parameters control how long to delay
  // producing NetworkChanged signal after particular input signals so as to
  // combine duplicates.  In other words if an input signal is repeated within
  // the corresponding delay period, only one resulting NetworkChange signal is
  // produced.
  struct NET_EXPORT NetworkChangeCalculatorParams {
    NetworkChangeCalculatorParams();
    // Controls delay after OnIPAddressChanged when transitioning from an
    // offline state.
    base::TimeDelta ip_address_offline_delay_;
    // Controls delay after OnIPAddressChanged when transitioning from an
    // online state.
    base::TimeDelta ip_address_online_delay_;
    // Controls delay after OnConnectionTypeChanged when transitioning from an
    // offline state.
    base::TimeDelta connection_type_offline_delay_;
    // Controls delay after OnConnectionTypeChanged when transitioning from an
    // online state.
    base::TimeDelta connection_type_online_delay_;
  };

  explicit NetworkChangeNotifier(
      const NetworkChangeCalculatorParams& params =
          NetworkChangeCalculatorParams());

#if defined(OS_LINUX)
  // Returns the AddressTrackerLinux if present.
  // TODO(szym): Retrieve AddressMap from NetworkState. http://crbug.com/144212
  virtual const internal::AddressTrackerLinux*
      GetAddressTrackerInternal() const;
#endif

  // See the description of NetworkChangeNotifier::GetMaxBandwidth().
  // Implementations must be thread-safe. Implementations must also be
  // cheap as it is called often.
  virtual double GetCurrentMaxBandwidth() const;

  // Returns a theoretical upper limit on download bandwidth given a connection
  // subtype. The mapping of connection type to maximum bandwidth is provided in
  // the NetInfo spec: http://w3c.github.io/netinfo/.
  static double GetMaxBandwidthForConnectionSubtype(ConnectionSubtype subtype);

  // Broadcasts a notification to all registered observers.  Note that this
  // happens asynchronously, even for observers on the current thread, even in
  // tests.
  static void NotifyObserversOfIPAddressChange();
  static void NotifyObserversOfConnectionTypeChange();
  static void NotifyObserversOfDNSChange();
  static void NotifyObserversOfNetworkChange(ConnectionType type);
  static void NotifyObserversOfMaxBandwidthChange(double max_bandwidth_mbps);

  // Stores |config| in NetworkState and notifies observers.
  static void SetDnsConfig(const DnsConfig& config);

 private:
  friend class HostResolverImplDnsTest;
  friend class NetworkChangeNotifierAndroidTest;
  friend class NetworkChangeNotifierLinuxTest;
  friend class NetworkChangeNotifierWinTest;

  class NetworkState;
  class NetworkChangeCalculator;

  void NotifyObserversOfIPAddressChangeImpl();
  void NotifyObserversOfConnectionTypeChangeImpl(ConnectionType type);
  void NotifyObserversOfDNSChangeImpl();
  void NotifyObserversOfNetworkChangeImpl(ConnectionType type);
  void NotifyObserversOfMaxBandwidthChangeImpl(double max_bandwidth_mbps);

  const scoped_refptr<ObserverListThreadSafe<IPAddressObserver>>
      ip_address_observer_list_;
  const scoped_refptr<ObserverListThreadSafe<ConnectionTypeObserver>>
      connection_type_observer_list_;
  const scoped_refptr<ObserverListThreadSafe<DNSObserver>>
      resolver_state_observer_list_;
  const scoped_refptr<ObserverListThreadSafe<NetworkChangeObserver>>
      network_change_observer_list_;
  const scoped_refptr<ObserverListThreadSafe<MaxBandwidthObserver>>
      max_bandwidth_observer_list_;

  // The current network state. Hosts DnsConfig, exposed via GetDnsConfig.
  scoped_ptr<NetworkState> network_state_;

  // A little-piggy-back observer that simply logs UMA histogram data.
  scoped_ptr<HistogramWatcher> histogram_watcher_;

  // Computes NetworkChange signal from IPAddress and ConnectionType signals.
  scoped_ptr<NetworkChangeCalculator> network_change_calculator_;

  // Set true to disable non-test notifications (to prevent flakes in tests).
  bool test_notifications_only_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifier);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_H_
