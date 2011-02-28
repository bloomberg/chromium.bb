// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides wifi scan API binding for suitable for typical linux distributions.
// Currently, only the NetworkManager API is used, accessed via D-Bus (in turn
// accessed via the GLib wrapper).

#include "content/browser/geolocation/wifi_data_provider_linux.h"

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <dlfcn.h>
#include <glib.h>

#include "base/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"

namespace {
// The time periods between successive polls of the wifi data.
const int kDefaultPollingIntervalMilliseconds = 10 * 1000;  // 10s
const int kNoChangePollingIntervalMilliseconds = 2 * 60 * 1000;  // 2 mins
const int kTwoNoChangePollingIntervalMilliseconds = 10 * 60 * 1000;  // 10 mins
const int kNoWifiPollingIntervalMilliseconds = 20 * 1000; // 20s

const char kNetworkManagerServiceName[] = "org.freedesktop.NetworkManager";
const char kNetworkManagerPath[] = "/org/freedesktop/NetworkManager";
const char kNetworkManagerInterface[] = "org.freedesktop.NetworkManager";

// From http://projects.gnome.org/NetworkManager/developers/spec.html
enum { NM_DEVICE_TYPE_WIFI = 2 };

// Function type matching dbus_g_bus_get_private so that we can
// dynamically determine the presence of this symbol (see
// NetworkManagerWlanApi::Init)
typedef DBusGConnection* DBusGBusGetPrivateFunc(
    DBusBusType type, GMainContext* context, GError** error);

// Utility wrappers to make various GLib & DBus structs into scoped objects.
class ScopedGPtrArrayFree {
 public:
  void operator()(GPtrArray* x) const {
    if (x)
      g_ptr_array_free(x, TRUE);
  }
};
// Use ScopedGPtrArrayPtr as if it were scoped_ptr<GPtrArray>
typedef scoped_ptr_malloc<GPtrArray, ScopedGPtrArrayFree> ScopedGPtrArrayPtr;

class ScopedGObjectFree {
 public:
  void operator()(void* x) const {
    if (x)
      g_object_unref(x);
  }
};
// Use ScopedDBusGProxyPtr as if it were scoped_ptr<DBusGProxy>
typedef scoped_ptr_malloc<DBusGProxy, ScopedGObjectFree> ScopedDBusGProxyPtr;

// Use ScopedGValue::v as an instance of GValue with automatic cleanup.
class ScopedGValue {
 public:
  ScopedGValue()
      : v(empty_gvalue()) {
  }
  ~ScopedGValue() {
    g_value_unset(&v);
  }
  static GValue empty_gvalue() {
    GValue value = {0};
    return value;
  }

  GValue v;
};

// Use ScopedDLHandle to automatically clean up after dlopen.
class ScopedDLHandle {
 public:
  ScopedDLHandle(void* handle)
      : handle_(handle) {
  }
  ~ScopedDLHandle() {
    if (handle_)
      dlclose(handle_);
  }
  void* get() {
    return handle_;
  }
 private:
  void *handle_;
};

// Wifi API binding to NetworkManager, to allow reuse of the polling behavior
// defined in WifiDataProviderCommon.
// TODO(joth): NetworkManager also allows for notification based handling,
// however this will require reworking of the threading code to run a GLib
// event loop (GMainLoop).
class NetworkManagerWlanApi : public WifiDataProviderCommon::WlanApiInterface {
 public:
  NetworkManagerWlanApi();
  ~NetworkManagerWlanApi();

  // Must be called before any other interface method. Will return false if the
  // NetworkManager session cannot be created (e.g. not present on this distro),
  // in which case no other method may be called.
  bool Init();

  // WifiDataProviderCommon::WlanApiInterface
  bool GetAccessPointData(WifiData::AccessPointDataSet* data);

 private:
  // Checks if the last dbus call returned an error.  If it did, logs the error
  // message, frees it and returns true.
  // This must be called after every dbus call that accepts |&error_|
  bool CheckError();

  // Enumerates the list of available network adapter devices known to
  // NetworkManager. Ownership of the array (and contained objects) is returned
  // to the caller.
  GPtrArray* GetAdapterDeviceList();

  // Given the NetworkManager path to a wireless adapater, dumps the wifi scan
  // results and appends them to |data|. Returns false if a fatal error is
  // encountered such that the data set could not be populated.
  bool GetAccessPointsForAdapter(const gchar* adapter_path,
                                 WifiData::AccessPointDataSet* data);

  // Internal method used by |GetAccessPointsForAdapter|, given a wifi access
  // point proxy retrieves the named property into |value_out|. Returns false if
  // the property could not be read, or is not of type |expected_gvalue_type|.
  bool GetAccessPointProperty(DBusGProxy* proxy, const char* property_name,
                              int expected_gvalue_type, GValue* value_out);

  // Error from the last dbus call.  NULL when there's no error.  Freed and
  // cleared by CheckError().
  GError* error_;
  // Connection to the dbus system bus.
  DBusGConnection* connection_;
  // Main context
  GMainContext* context_;
  // Proxy to the network manager dbus service.
  ScopedDBusGProxyPtr proxy_;

  DISALLOW_COPY_AND_ASSIGN(NetworkManagerWlanApi);
};

// Convert a wifi frequency to the corresponding channel. Adapted from
// geolocaiton/wifilib.cc in googleclient (internal to google).
int frquency_in_khz_to_channel(int frequency_khz) {
  if (frequency_khz >= 2412000 && frequency_khz <= 2472000)  // Channels 1-13.
    return (frequency_khz - 2407000) / 5000;
  if (frequency_khz == 2484000)
    return 14;
  if (frequency_khz > 5000000 && frequency_khz < 6000000)  // .11a bands.
    return (frequency_khz - 5000000) / 5000;
  // Ignore everything else.
  return AccessPointData().channel;  // invalid channel
}

NetworkManagerWlanApi::NetworkManagerWlanApi()
    : error_(NULL),
      connection_(NULL),
      context_(NULL)
{
}

NetworkManagerWlanApi::~NetworkManagerWlanApi() {
  proxy_.reset();
  if (connection_) {
    dbus_connection_close(dbus_g_connection_get_connection(connection_));
    dbus_g_connection_unref(connection_);
  }
  if (context_)
    g_main_context_unref(context_);

  DCHECK(!error_) << "Missing a call to CheckError() to clear |error_|";
}

bool NetworkManagerWlanApi::Init() {
  // Chrome DLL init code handles initializing the thread system, so rather than
  // get caught up with that nonsense here, lets just assert our requirement.
  CHECK(g_thread_supported());

  // We require a private bus to ensure that we don't interfere with the
  // default loop on the main thread.  Unforunately this functionality is only
  // available in dbus-glib-0.84 and later. We do a dynamic symbol lookup
  // to determine if dbus_g_bus_get_private is available. See bug
  // http://code.google.com/p/chromium/issues/detail?id=59913 for more
  // information.
  ScopedDLHandle handle(dlopen(NULL, RTLD_LAZY));
  if (!handle.get())
    return false;

  DBusGBusGetPrivateFunc *my_dbus_g_bus_get_private =
      (DBusGBusGetPrivateFunc *) dlsym(handle.get(), "dbus_g_bus_get_private");

  if (!my_dbus_g_bus_get_private) {
    LOG(ERROR) << "We need dbus-glib >= 0.84 for wifi geolocation.";
    return false;
  }
  // Get a private connection to the session bus.
  context_ = g_main_context_new();
  DCHECK(context_);
  connection_ = my_dbus_g_bus_get_private(DBUS_BUS_SYSTEM, context_, &error_);

  if (CheckError())
    return false;
  DCHECK(connection_);

  // Disable timers on the connection since we don't need them and
  // we're not going to run them anyway as the connection is associated
  // with a private context. See bug http://crbug.com/40803
  dbus_bool_t ok = dbus_connection_set_timeout_functions(
      dbus_g_connection_get_connection(connection_),
      NULL, NULL, NULL, NULL, NULL);
  DCHECK(ok);

  proxy_.reset(dbus_g_proxy_new_for_name(connection_,
                                         kNetworkManagerServiceName,
                                         kNetworkManagerPath,
                                         kNetworkManagerInterface));
  DCHECK(proxy_.get());

  // Validate the proxy object by checking we can enumerate devices.
  ScopedGPtrArrayPtr device_list(GetAdapterDeviceList());
  return !!device_list.get();
}

bool NetworkManagerWlanApi::GetAccessPointData(
    WifiData::AccessPointDataSet* data) {
  ScopedGPtrArrayPtr device_list(GetAdapterDeviceList());
  if (device_list == NULL) {
    DLOG(WARNING) << "Could not enumerate access points";
    return false;
  }
  int success_count = 0;
  int fail_count = 0;

  // Iterate the devices, getting APs for each wireless adapter found
  for (guint i = 0; i < device_list->len; i++) {
    const gchar* device_path =
        reinterpret_cast<const gchar*>(g_ptr_array_index(device_list, i));

    ScopedDBusGProxyPtr device_properties_proxy(dbus_g_proxy_new_from_proxy(
        proxy_.get(), DBUS_INTERFACE_PROPERTIES, device_path));
    ScopedGValue device_type_g_value;
    dbus_g_proxy_call(device_properties_proxy.get(), "Get",  &error_,
                      G_TYPE_STRING, "org.freedesktop.NetworkManager.Device",
                      G_TYPE_STRING, "DeviceType",
                      G_TYPE_INVALID,
                      G_TYPE_VALUE,   &device_type_g_value.v,
                      G_TYPE_INVALID);
    if (CheckError())
      continue;

    const guint device_type = g_value_get_uint(&device_type_g_value.v);

    if (device_type == NM_DEVICE_TYPE_WIFI) {  // Found a wlan adapter
      if (GetAccessPointsForAdapter(device_path, data))
        ++success_count;
      else
        ++fail_count;
    }
  }
  // At least one successfull scan overrides any other adapter reporting error.
  return success_count || fail_count == 0;
}

bool NetworkManagerWlanApi::CheckError() {
  if (error_) {
    LOG(ERROR) << "Failed to complete NetworkManager call: " << error_->message;
    g_error_free(error_);
    error_ = NULL;
    return true;
  }
  return false;
}

GPtrArray* NetworkManagerWlanApi::GetAdapterDeviceList() {
  GPtrArray* device_list = NULL;
  dbus_g_proxy_call(proxy_.get(), "GetDevices", &error_,
                    G_TYPE_INVALID,
                    dbus_g_type_get_collection("GPtrArray",
                                               DBUS_TYPE_G_OBJECT_PATH),
                    &device_list,
                    G_TYPE_INVALID);
  if (CheckError())
    return NULL;
  return device_list;
}

bool NetworkManagerWlanApi::GetAccessPointsForAdapter(
    const gchar* adapter_path, WifiData::AccessPointDataSet* data) {
  DCHECK(proxy_.get());

  // Create a proxy object for this wifi adapter, and ask it to do a scan
  // (or at least, dump its scan results).
  ScopedDBusGProxyPtr wifi_adapter_proxy(dbus_g_proxy_new_from_proxy(
      proxy_.get(), "org.freedesktop.NetworkManager.Device.Wireless",
      adapter_path));

  GPtrArray* ap_list_raw = NULL;
  // Enumerate the access points for this adapter.
  dbus_g_proxy_call(wifi_adapter_proxy.get(), "GetAccessPoints",  &error_,
                    G_TYPE_INVALID,
                    dbus_g_type_get_collection("GPtrArray",
                                               DBUS_TYPE_G_OBJECT_PATH),
                    &ap_list_raw,
                    G_TYPE_INVALID);
  ScopedGPtrArrayPtr ap_list(ap_list_raw);  // Takes ownership.
  ap_list_raw = NULL;

  if (CheckError())
    return false;

  DVLOG(1) << "Wireless adapter " << adapter_path << " found " << ap_list->len
           << " access points.";

  for (guint i = 0; i < ap_list->len; i++) {
    const gchar* ap_path =
        reinterpret_cast<const gchar*>(g_ptr_array_index(ap_list, i));
    ScopedDBusGProxyPtr access_point_proxy(dbus_g_proxy_new_from_proxy(
        proxy_.get(), DBUS_INTERFACE_PROPERTIES, ap_path));

    AccessPointData access_point_data;
    {  // Read SSID.
      ScopedGValue ssid_g_value;
      if (!GetAccessPointProperty(access_point_proxy.get(), "Ssid",
                                  G_TYPE_BOXED, &ssid_g_value.v))
        continue;
      const GArray* ssid =
          reinterpret_cast<const GArray*>(g_value_get_boxed(&ssid_g_value.v));
      UTF8ToUTF16(ssid->data, ssid->len, &access_point_data.ssid);
    }

    { // Read the mac address
      ScopedGValue mac_g_value;
      if (!GetAccessPointProperty(access_point_proxy.get(), "HwAddress",
                                  G_TYPE_STRING, &mac_g_value.v))
        continue;
      std::string mac = g_value_get_string(&mac_g_value.v);
      ReplaceSubstringsAfterOffset(&mac, 0U, ":", "");
      std::vector<uint8> mac_bytes;
      if (!base::HexStringToBytes(mac, &mac_bytes) || mac_bytes.size() != 6) {
        DLOG(WARNING) << "Can't parse mac address (found " << mac_bytes.size()
                      << " bytes) so using raw string: " << mac;
        access_point_data.mac_address = UTF8ToUTF16(mac);
      } else {
        access_point_data.mac_address = MacAddressAsString16(&mac_bytes[0]);
      }
    }

    {  // Read signal strength.
      ScopedGValue signal_g_value;
      if (!GetAccessPointProperty(access_point_proxy.get(), "Strength",
                                  G_TYPE_UCHAR, &signal_g_value.v))
        continue;
      // Convert strength as a percentage into dBs.
      access_point_data.radio_signal_strength =
          -100 + g_value_get_uchar(&signal_g_value.v) / 2;
    }

    { // Read the channel
      ScopedGValue freq_g_value;
      if (!GetAccessPointProperty(access_point_proxy.get(), "Frequency",
                                  G_TYPE_UINT, &freq_g_value.v))
        continue;
      // NetworkManager returns frequency in MHz.
      access_point_data.channel =
          frquency_in_khz_to_channel(g_value_get_uint(&freq_g_value.v) * 1000);
    }
    data->insert(access_point_data);
  }
  return true;
}

bool NetworkManagerWlanApi::GetAccessPointProperty(DBusGProxy* proxy,
                                                   const char* property_name,
                                                   int expected_gvalue_type,
                                                   GValue* value_out) {
  dbus_g_proxy_call(proxy, "Get", &error_,
                    G_TYPE_STRING, "org.freedesktop.NetworkManager.AccessPoint",
                    G_TYPE_STRING, property_name,
                    G_TYPE_INVALID,
                    G_TYPE_VALUE, value_out,
                    G_TYPE_INVALID);
  if (CheckError())
    return false;
  if (!G_VALUE_HOLDS(value_out, expected_gvalue_type)) {
    DLOG(WARNING) << "Property " << property_name << " unexptected type "
                  << G_VALUE_TYPE(value_out);
    return false;
  }
  return true;
}

}  // namespace

// static
template<>
WifiDataProviderImplBase* WifiDataProvider::DefaultFactoryFunction() {
  return new WifiDataProviderLinux();
}

WifiDataProviderLinux::WifiDataProviderLinux() {
}

WifiDataProviderLinux::~WifiDataProviderLinux() {
}

WifiDataProviderCommon::WlanApiInterface*
WifiDataProviderLinux::NewWlanApi() {
  scoped_ptr<NetworkManagerWlanApi> wlan_api(new NetworkManagerWlanApi);
  if (wlan_api->Init())
    return wlan_api.release();
  return NULL;
}

PollingPolicyInterface* WifiDataProviderLinux::NewPollingPolicy() {
  return new GenericPollingPolicy<kDefaultPollingIntervalMilliseconds,
                                  kNoChangePollingIntervalMilliseconds,
                                  kTwoNoChangePollingIntervalMilliseconds,
                                  kNoWifiPollingIntervalMilliseconds>;
}
