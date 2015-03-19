// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_alsa.h"

#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "media/midi/midi_port_info.h"

namespace media {

namespace {

// Per-output buffer. This can be smaller, but then large sysex messages
// will be (harmlessly) split across multiple seq events. This should
// not have any real practical effect, except perhaps to slightly reorder
// realtime messages with respect to sysex.
const size_t kSendBufferSize = 256;

// Minimum client id for which we will have ALSA card devices for. When we
// are searching for card devices (used to get the path, id, and manufacturer),
// we don't want to get confused by kernel clients that do not have a card.
// See seq_clientmgr.c in the ALSA code for this.
// TODO(agoode): Add proper client -> card export from the kernel to avoid
//               hardcoding.
const int kMinimumClientIdForCards = 16;

// udev key constants.
const char kSoundClass[] = "sound";
const char kIdVendor[] = "ID_VENDOR";
const char kIdVendorEnc[] = "ID_VENDOR_ENC";
const char kIdVendorFromDatabase[] = "ID_VENDOR_FROM_DATABASE";
const char kSysattrVendorName[] = "vendor_name";
const char kIdVendorId[] = "ID_VENDOR_ID";
const char kSysattrVendor[] = "vendor";
const char kIdModelId[] = "ID_MODEL_ID";
const char kSysattrModel[] = "model";
const char kIdBus[] = "ID_BUS";
const char kIdPath[] = "ID_PATH";
const char kUsbInterfaceNum[] = "ID_USB_INTERFACE_NUM";

// ALSA constants.
const char kAlsaHw[] = "hw";

// Constants for the capabilities we search for in inputs and outputs.
// See http://www.alsa-project.org/alsa-doc/alsa-lib/seq.html.
const unsigned int kRequiredInputPortCaps =
    SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
const unsigned int kRequiredOutputPortCaps =
    SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;

int AddrToInt(const snd_seq_addr_t* addr) {
  return (addr->client << 8) | addr->port;
}

// TODO(agoode): Move this to device/udev_linux.
#if defined(USE_UDEV)
const std::string UdevDeviceGetPropertyOrSysattr(
    struct udev_device* udev_device,
    const char* property_key,
    const char* sysattr_key) {
  // First try the property.
  std::string value =
      device::UdevDeviceGetPropertyValue(udev_device, property_key);

  // If no property, look for sysattrs and walk up the parent devices too.
  while (value.empty() && udev_device) {
    value = device::UdevDeviceGetSysattrValue(udev_device, sysattr_key);
    udev_device = device::udev_device_get_parent(udev_device);
  }
  return value;
}
#endif  // defined(USE_UDEV)

}  // namespace

MidiManagerAlsa::MidiManagerAlsa()
    : in_client_(NULL),
      out_client_(NULL),
      out_client_id_(-1),
      in_port_(-1),
      decoder_(NULL),
#if defined(USE_UDEV)
      udev_(device::udev_new()),
#endif  // defined(USE_UDEV)
      send_thread_("MidiSendThread"),
      event_thread_("MidiEventThread"),
      event_thread_shutdown_(false) {
  // Initialize decoder.
  snd_midi_event_new(0, &decoder_);
  snd_midi_event_no_status(decoder_, 1);
}

MidiManagerAlsa::~MidiManagerAlsa() {
  // Tell the event thread it will soon be time to shut down. This gives
  // us assurance the thread will stop in case the SND_SEQ_EVENT_CLIENT_EXIT
  // message is lost.
  {
    base::AutoLock lock(shutdown_lock_);
    event_thread_shutdown_ = true;
  }

  // Stop the send thread.
  send_thread_.Stop();

  // Close the out client. This will trigger the event thread to stop,
  // because of SND_SEQ_EVENT_CLIENT_EXIT.
  if (out_client_)
    snd_seq_close(out_client_);

  // Wait for the event thread to stop.
  event_thread_.Stop();

  // Close the in client.
  if (in_client_)
    snd_seq_close(in_client_);

  // Free the decoder.
  snd_midi_event_free(decoder_);
}

void MidiManagerAlsa::StartInitialization() {
  // TODO(agoode): Move off I/O thread. See http://crbug.com/374341.

  // Create client handles.
  int err = snd_seq_open(&in_client_, kAlsaHw, SND_SEQ_OPEN_INPUT, 0);
  if (err != 0) {
    VLOG(1) << "snd_seq_open fails: " << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }
  int in_client_id = snd_seq_client_id(in_client_);
  err = snd_seq_open(&out_client_, kAlsaHw, SND_SEQ_OPEN_OUTPUT, 0);
  if (err != 0) {
    VLOG(1) << "snd_seq_open fails: " << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }
  out_client_id_ = snd_seq_client_id(out_client_);

  // Name the clients.
  err = snd_seq_set_client_name(in_client_, "Chrome (input)");
  if (err != 0) {
    VLOG(1) << "snd_seq_set_client_name fails: " << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }
  err = snd_seq_set_client_name(out_client_, "Chrome (output)");
  if (err != 0) {
    VLOG(1) << "snd_seq_set_client_name fails: " << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }

  // Create input port.
  in_port_ = snd_seq_create_simple_port(in_client_, NULL,
                                        SND_SEQ_PORT_CAP_WRITE |
                                        SND_SEQ_PORT_CAP_NO_EXPORT,
                                        SND_SEQ_PORT_TYPE_MIDI_GENERIC |
                                        SND_SEQ_PORT_TYPE_APPLICATION);
  if (in_port_ < 0) {
    VLOG(1) << "snd_seq_create_simple_port fails: " << snd_strerror(in_port_);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }

  // Subscribe to the announce port.
  snd_seq_port_subscribe_t* subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_addr_t announce_sender;
  snd_seq_addr_t announce_dest;
  announce_sender.client = SND_SEQ_CLIENT_SYSTEM;
  announce_sender.port = SND_SEQ_PORT_SYSTEM_ANNOUNCE;
  announce_dest.client = in_client_id;
  announce_dest.port = in_port_;
  snd_seq_port_subscribe_set_sender(subs, &announce_sender);
  snd_seq_port_subscribe_set_dest(subs, &announce_dest);
  err = snd_seq_subscribe_port(in_client_, subs);
  if (err != 0) {
    VLOG(1) << "snd_seq_subscribe_port on the announce port fails: "
            << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }

  EnumeratePorts();

  event_thread_.Start();
  event_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MidiManagerAlsa::ScheduleEventLoop, base::Unretained(this)));

  CompleteInitialization(MIDI_OK);
}

void MidiManagerAlsa::DispatchSendMidiData(MidiManagerClient* client,
                                           uint32 port_index,
                                           const std::vector<uint8>& data,
                                           double timestamp) {
  if (out_ports_.size() <= port_index)
    return;

  // Not correct right now. http://crbug.com/374341.
  if (!send_thread_.IsRunning())
    send_thread_.Start();

  base::TimeDelta delay;
  if (timestamp != 0.0) {
    base::TimeTicks time_to_send =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(
                                timestamp * base::Time::kMicrosecondsPerSecond);
    delay = std::max(time_to_send - base::TimeTicks::Now(), base::TimeDelta());
  }

  send_thread_.message_loop()->PostDelayedTask(
      FROM_HERE, base::Bind(&MidiManagerAlsa::SendMidiData,
                            base::Unretained(this), port_index, data),
      delay);

  // Acknowledge send.
  send_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&MidiManagerClient::AccumulateMidiBytesSent,
                            base::Unretained(client), data.size()));
}

MidiManagerAlsa::AlsaRawmidi::AlsaRawmidi(const MidiManagerAlsa* outer,
                                          const std::string& alsa_name,
                                          const std::string& alsa_longname,
                                          const std::string& alsa_driver,
                                          int card_index)
    : alsa_name_(alsa_name),
      alsa_longname_(alsa_longname),
      alsa_driver_(alsa_driver) {
  // Get udev properties if available.
  std::string vendor;
  std::string vendor_from_database;

#if defined(USE_UDEV)
  const std::string sysname = base::StringPrintf("card%i", card_index);
  device::ScopedUdevDevicePtr udev_device(
      device::udev_device_new_from_subsystem_sysname(
          outer->udev_.get(), kSoundClass, sysname.c_str()));

  // TODO(agoode): Move this to a new utility class in device/udev_linux?

  // Try to get the vendor string. Sometimes it is encoded.
  vendor = device::UdevDecodeString(
      device::UdevDeviceGetPropertyValue(udev_device.get(), kIdVendorEnc));
  // Sometimes it is not encoded.
  if (vendor.empty())
    UdevDeviceGetPropertyOrSysattr(udev_device.get(), kIdVendor,
                                   kSysattrVendorName);
  // Also get the vendor string from the hardware database.
  vendor_from_database = device::UdevDeviceGetPropertyValue(
      udev_device.get(), kIdVendorFromDatabase);

  // Get the device path.
  path_ = device::UdevDeviceGetPropertyValue(udev_device.get(), kIdPath);

  // Get the bus.
  bus_ = device::UdevDeviceGetPropertyValue(udev_device.get(), kIdBus);

  // Get the vendor id, by either property or sysattr.
  vendor_id_ = UdevDeviceGetPropertyOrSysattr(udev_device.get(), kIdVendorId,
                                              kSysattrVendor);

  // Get the model id, by either property or sysattr.
  model_id_ = UdevDeviceGetPropertyOrSysattr(udev_device.get(), kIdModelId,
                                             kSysattrModel);

  // Get the usb interface number.
  usb_interface_num_ =
      device::UdevDeviceGetPropertyValue(udev_device.get(), kUsbInterfaceNum);
#endif  // defined(USE_UDEV)

  manufacturer_ = ExtractManufacturerString(
      vendor, vendor_id_, vendor_from_database, alsa_name, alsa_longname);
}

MidiManagerAlsa::AlsaRawmidi::~AlsaRawmidi() {
}

const std::string MidiManagerAlsa::AlsaRawmidi::alsa_name() const {
  return alsa_name_;
}

const std::string MidiManagerAlsa::AlsaRawmidi::alsa_longname() const {
  return alsa_longname_;
}

const std::string MidiManagerAlsa::AlsaRawmidi::manufacturer() const {
  return manufacturer_;
}

const std::string MidiManagerAlsa::AlsaRawmidi::alsa_driver() const {
  return alsa_driver_;
}

const std::string MidiManagerAlsa::AlsaRawmidi::path() const {
  return path_;
}

const std::string MidiManagerAlsa::AlsaRawmidi::bus() const {
  return bus_;
}

const std::string MidiManagerAlsa::AlsaRawmidi::vendor_id() const {
  return vendor_id_;
}

const std::string MidiManagerAlsa::AlsaRawmidi::id() const {
  std::string id = vendor_id_;
  if (!model_id_.empty())
    id += ":" + model_id_;
  if (!usb_interface_num_.empty())
    id += ":" + usb_interface_num_;
  return id;
}

// static
std::string MidiManagerAlsa::AlsaRawmidi::ExtractManufacturerString(
    const std::string& udev_id_vendor,
    const std::string& udev_id_vendor_id,
    const std::string& udev_id_vendor_from_database,
    const std::string& alsa_name,
    const std::string& alsa_longname) {
  // Let's try to determine the manufacturer. Here is the ordered preference
  // in extraction:
  //  1. Vendor name from the hardware device string, from udev properties
  //     or sysattrs.
  //  2. Vendor name from the udev database (property ID_VENDOR_FROM_DATABASE).
  //  3. Heuristic from ALSA.

  // Is the vendor string not just the vendor hex id?
  if (udev_id_vendor != udev_id_vendor_id) {
    return udev_id_vendor;
  }

  // Is there a vendor string in the hardware database?
  if (!udev_id_vendor_from_database.empty()) {
    return udev_id_vendor_from_database;
  }

  // Ok, udev gave us nothing useful, or was unavailable. So try a heuristic.
  // We assume that card longname is in the format of
  // "<manufacturer> <name> at <bus>". Otherwise, we give up to detect
  // a manufacturer name here.
  size_t at_index = alsa_longname.rfind(" at ");
  if (at_index && at_index != std::string::npos) {
    size_t name_index = alsa_longname.rfind(alsa_name, at_index - 1);
    if (name_index && name_index != std::string::npos)
      return alsa_longname.substr(0, name_index - 1);
  }

  // Failure.
  return "";
}

MidiManagerAlsa::AlsaPortMetadata::AlsaPortMetadata(
    const std::string& path,
    const std::string& bus,
    const std::string& id,
    const snd_seq_addr_t* address,
    const std::string& client_name,
    const std::string& port_name,
    const std::string& card_name,
    const std::string& card_longname,
    Type type)
    : path_(path),
      bus_(bus),
      id_(id),
      client_addr_(address->client),
      port_addr_(address->port),
      client_name_(client_name),
      port_name_(port_name),
      card_name_(card_name),
      card_longname_(card_longname),
      type_(type) {
}

MidiManagerAlsa::AlsaPortMetadata::~AlsaPortMetadata() {
}

scoped_ptr<base::Value> MidiManagerAlsa::AlsaPortMetadata::Value() const {
  base::DictionaryValue* value = new base::DictionaryValue();
  value->SetString("path", path_);
  value->SetString("bus", bus_);
  value->SetString("id", id_);
  value->SetInteger("clientAddr", client_addr_);
  value->SetInteger("portAddr", port_addr_);
  value->SetString("clientName", client_name_);
  value->SetString("portName", port_name_);
  value->SetString("cardName", card_name_);
  value->SetString("cardLongname", card_longname_);
  std::string type;
  switch (type_) {
    case Type::kInput:
      type = "input";
      break;

    case Type::kOutput:
      type = "output";
      break;
  }
  value->SetString("type", type);

  return scoped_ptr<base::Value>(value).Pass();
}

std::string MidiManagerAlsa::AlsaPortMetadata::JSONValue() const {
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.Serialize(*Value().get());
  return json;
}

// TODO(agoode): Do not use SHA256 here. Instead store a persistent
//               mapping and just use a UUID or other random string.
//               http://crbug.com/465320
std::string MidiManagerAlsa::AlsaPortMetadata::OpaqueKey() const {
  uint8 hash[crypto::kSHA256Length];
  crypto::SHA256HashString(JSONValue(), &hash, sizeof(hash));
  return base::HexEncode(&hash, sizeof(hash));
}

// TODO(agoode): Add a client->card/rawmidi mapping to the kernel to avoid
//               needing to probe in this way.
ScopedVector<MidiManagerAlsa::AlsaRawmidi> MidiManagerAlsa::AllAlsaRawmidis() {
  ScopedVector<AlsaRawmidi> devices;
  snd_ctl_card_info_t* card;
  snd_rawmidi_info_t* midi_out;
  snd_rawmidi_info_t* midi_in;
  snd_ctl_card_info_alloca(&card);
  snd_rawmidi_info_alloca(&midi_out);
  snd_rawmidi_info_alloca(&midi_in);
  for (int card_index = -1; !snd_card_next(&card_index) && card_index >= 0;) {
    const std::string id = base::StringPrintf("hw:CARD=%i", card_index);
    snd_ctl_t* handle;
    int err = snd_ctl_open(&handle, id.c_str(), 0);
    if (err != 0) {
      VLOG(1) << "snd_ctl_open fails: " << snd_strerror(err);
      continue;
    }
    err = snd_ctl_card_info(handle, card);
    if (err != 0) {
      VLOG(1) << "snd_ctl_card_info fails: " << snd_strerror(err);
      snd_ctl_close(handle);
      continue;
    }
    // Enumerate any rawmidi devices (not subdevices) and extract AlsaRawmidi.
    for (int device = -1;
         !snd_ctl_rawmidi_next_device(handle, &device) && device >= 0;) {
      bool output;
      bool input;
      snd_rawmidi_info_set_device(midi_out, device);
      snd_rawmidi_info_set_subdevice(midi_out, 0);
      snd_rawmidi_info_set_stream(midi_out, SND_RAWMIDI_STREAM_OUTPUT);
      output = snd_ctl_rawmidi_info(handle, midi_out) == 0;
      snd_rawmidi_info_set_device(midi_in, device);
      snd_rawmidi_info_set_subdevice(midi_in, 0);
      snd_rawmidi_info_set_stream(midi_in, SND_RAWMIDI_STREAM_INPUT);
      input = snd_ctl_rawmidi_info(handle, midi_in) == 0;
      if (!output && !input)
        continue;

      // Compute and save ALSA and udev properties.
      snd_rawmidi_info_t* midi = midi_out ? midi_out : midi_in;
      devices.push_back(new AlsaRawmidi(this, snd_rawmidi_info_get_name(midi),
                                        snd_ctl_card_info_get_longname(card),
                                        snd_ctl_card_info_get_driver(card),
                                        card_index));
    }
    snd_ctl_close(handle);
  }

  return devices.Pass();
}

void MidiManagerAlsa::EnumeratePorts() {
  ScopedVector<AlsaRawmidi> devices = AllAlsaRawmidis();

  snd_seq_port_subscribe_t* subs;
  snd_seq_port_subscribe_alloca(&subs);

  int in_client_id = snd_seq_client_id(in_client_);

  // Enumerate all clients.
  snd_seq_client_info_t* client_info;
  snd_seq_client_info_alloca(&client_info);
  snd_seq_port_info_t* port_info;
  snd_seq_port_info_alloca(&port_info);

  // Enumerate clients.
  snd_seq_client_info_set_client(client_info, -1);
  uint32 current_input = 0;
  unsigned int current_device = 0;
  while (!snd_seq_query_next_client(in_client_, client_info)) {
    int client_id = snd_seq_client_info_get_client(client_info);
    if ((client_id == in_client_id) || (client_id == out_client_id_)) {
      // Skip our own clients.
      continue;
    }

    // Get client metadata.
    const std::string client_name = snd_seq_client_info_get_name(client_info);
    snd_seq_port_info_set_client(port_info, client_id);
    snd_seq_port_info_set_port(port_info, -1);

    std::string manufacturer;
    std::string driver;
    std::string path;
    std::string bus;
    std::string vendor_id;
    std::string id;
    std::string card_name;
    std::string card_longname;

    // Join kernel clients against the list of AlsaRawmidis.
    // In the current ALSA kernel implementation, kernel clients match the
    // kernel devices in the same order, for devices with client_id over
    // kMinimumClientIdForCards.
    if ((snd_seq_client_info_get_type(client_info) == SND_SEQ_KERNEL_CLIENT) &&
        (current_device < devices.size()) &&
        (client_id >= kMinimumClientIdForCards)) {
      const AlsaRawmidi* device = devices[current_device];
      manufacturer = device->manufacturer();
      driver = device->alsa_driver();
      path = device->path();
      bus = device->bus();
      vendor_id = device->vendor_id();
      id = device->id();
      card_name = device->alsa_name();
      card_longname = device->alsa_longname();
      current_device++;
    }
    // Enumerate ports.
    while (!snd_seq_query_next_port(in_client_, port_info)) {
      unsigned int port_type = snd_seq_port_info_get_type(port_info);
      if (port_type & SND_SEQ_PORT_TYPE_MIDI_GENERIC) {
        const snd_seq_addr_t* addr = snd_seq_port_info_get_addr(port_info);
        const std::string name = snd_seq_port_info_get_name(port_info);
        std::string version;
        if (!driver.empty()) {
          version = driver + " / ";
        }
        version += base::StringPrintf("ALSA library version %d.%d.%d",
                                      SND_LIB_MAJOR,
                                      SND_LIB_MINOR,
                                      SND_LIB_SUBMINOR);
        unsigned int caps = snd_seq_port_info_get_capability(port_info);
        if ((caps & kRequiredInputPortCaps) == kRequiredInputPortCaps) {
          // Subscribe to this port.
          const snd_seq_addr_t* sender = snd_seq_port_info_get_addr(port_info);
          snd_seq_addr_t dest;
          dest.client = snd_seq_client_id(in_client_);
          dest.port = in_port_;
          snd_seq_port_subscribe_set_sender(subs, sender);
          snd_seq_port_subscribe_set_dest(subs, &dest);
          int err = snd_seq_subscribe_port(in_client_, subs);
          if (err != 0) {
            VLOG(1) << "snd_seq_subscribe_port fails: " << snd_strerror(err);
          } else {
            source_map_[AddrToInt(sender)] = current_input++;
            const AlsaPortMetadata metadata(path, bus, id, addr, client_name,
                                            name, card_name, card_longname,
                                            AlsaPortMetadata::Type::kInput);
            const std::string id = metadata.OpaqueKey();
            AddInputPort(MidiPortInfo(id.c_str(), manufacturer, name, version,
                                      MIDI_PORT_OPENED));
          }
        }
        if ((caps & kRequiredOutputPortCaps) == kRequiredOutputPortCaps) {
          // Create a port for us to send on.
          int out_port =
              snd_seq_create_simple_port(out_client_, NULL,
                                         SND_SEQ_PORT_CAP_READ |
                                         SND_SEQ_PORT_CAP_NO_EXPORT,
                                         SND_SEQ_PORT_TYPE_MIDI_GENERIC |
                                         SND_SEQ_PORT_TYPE_APPLICATION);
          if (out_port < 0) {
            VLOG(1) << "snd_seq_create_simple_port fails: "
                    << snd_strerror(out_port);
            // Skip this output port for now.
            continue;
          }

          // Activate port subscription.
          snd_seq_addr_t sender;
          const snd_seq_addr_t* dest = snd_seq_port_info_get_addr(port_info);
          sender.client = snd_seq_client_id(out_client_);
          sender.port = out_port;
          snd_seq_port_subscribe_set_sender(subs, &sender);
          snd_seq_port_subscribe_set_dest(subs, dest);
          int err = snd_seq_subscribe_port(out_client_, subs);
          if (err != 0) {
            VLOG(1) << "snd_seq_subscribe_port fails: " << snd_strerror(err);
            snd_seq_delete_simple_port(out_client_, out_port);
          } else {
            out_ports_.push_back(out_port);
            const AlsaPortMetadata metadata(path, bus, id, addr, client_name,
                                            name, card_name, card_longname,
                                            AlsaPortMetadata::Type::kOutput);
            const std::string id = metadata.OpaqueKey();
            AddOutputPort(MidiPortInfo(id.c_str(), manufacturer, name, version,
                                       MIDI_PORT_OPENED));
          }
        }
      }
    }
  }
}

void MidiManagerAlsa::SendMidiData(uint32 port_index,
                                   const std::vector<uint8>& data) {
  DCHECK(send_thread_.message_loop_proxy()->BelongsToCurrentThread());

  snd_midi_event_t* encoder;
  snd_midi_event_new(kSendBufferSize, &encoder);
  for (unsigned int i = 0; i < data.size(); i++) {
    snd_seq_event_t event;
    int result = snd_midi_event_encode_byte(encoder, data[i], &event);
    if (result == 1) {
      // Full event, send it.
      snd_seq_ev_set_source(&event, out_ports_[port_index]);
      snd_seq_ev_set_subs(&event);
      snd_seq_ev_set_direct(&event);
      snd_seq_event_output_direct(out_client_, &event);
    }
  }
  snd_midi_event_free(encoder);
}

void MidiManagerAlsa::ScheduleEventLoop() {
  event_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&MidiManagerAlsa::EventLoop, base::Unretained(this)));
}

void MidiManagerAlsa::EventLoop() {
  // Read available incoming MIDI data.
  snd_seq_event_t* event;
  int err = snd_seq_event_input(in_client_, &event);
  double timestamp = (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();

  // Handle errors.
  if (err == -ENOSPC) {
    VLOG(1) << "snd_seq_event_input detected buffer overrun";
    // We've lost events: check another way to see if we need to shut down.
    base::AutoLock lock(shutdown_lock_);
    if (!event_thread_shutdown_)
      ScheduleEventLoop();
    return;
  } else if (err < 0) {
    VLOG(1) << "snd_seq_event_input fails: " << snd_strerror(err);
    // TODO(agoode): Use RecordAction() or similar to log this.
    return;
  }

  // Handle announce events.
  if (event->source.client == SND_SEQ_CLIENT_SYSTEM &&
      event->source.port == SND_SEQ_PORT_SYSTEM_ANNOUNCE) {
    switch (event->type) {
      case SND_SEQ_EVENT_CLIENT_START:
        // TODO(agoode): rescan hardware devices.
        break;

      case SND_SEQ_EVENT_CLIENT_EXIT:
        // Check for disconnection of our "out" client. This means "shut down".
        if (event->data.addr.client == out_client_id_)
          return;

        // TODO(agoode): remove all ports for a client.
        break;

      case SND_SEQ_EVENT_PORT_START:
        // TODO(agoode): add port.
        break;

      case SND_SEQ_EVENT_PORT_EXIT:
        // TODO(agoode): remove port.
        break;
    }
  }

  ProcessSingleEvent(event, timestamp);

  // Do again.
  ScheduleEventLoop();
}

void MidiManagerAlsa::ProcessSingleEvent(snd_seq_event_t* event,
                                         double timestamp) {
  std::map<int, uint32>::iterator source_it =
      source_map_.find(AddrToInt(&event->source));
  if (source_it != source_map_.end()) {
    uint32 source = source_it->second;
    if (event->type == SND_SEQ_EVENT_SYSEX) {
      // Special! Variable-length sysex.
      ReceiveMidiData(source, static_cast<const uint8*>(event->data.ext.ptr),
                      event->data.ext.len, timestamp);
    } else {
      // Otherwise, decode this and send that on.
      unsigned char buf[12];
      long count = snd_midi_event_decode(decoder_, buf, sizeof(buf), event);
      if (count <= 0) {
        if (count != -ENOENT) {
          // ENOENT means that it's not a MIDI message, which is not an
          // error, but other negative values are errors for us.
          VLOG(1) << "snd_midi_event_decoder fails " << snd_strerror(count);
          // TODO(agoode): Record this failure.
        }
      } else {
        ReceiveMidiData(source, buf, count, timestamp);
      }
    }
  }
}

MidiManager* MidiManager::Create() {
  return new MidiManagerAlsa();
}

}  // namespace media
