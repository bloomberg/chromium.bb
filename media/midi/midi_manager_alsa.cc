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
namespace midi {

namespace {

// Per-output buffer. This can be smaller, but then large sysex messages
// will be (harmlessly) split across multiple seq events. This should
// not have any real practical effect, except perhaps to slightly reorder
// realtime messages with respect to sysex.
const size_t kSendBufferSize = 256;

// ALSA constants.
const char kAlsaHw[] = "hw";

// Constants for the capabilities we search for in inputs and outputs.
// See http://www.alsa-project.org/alsa-doc/alsa-lib/seq.html.
const unsigned int kRequiredInputPortCaps =
    SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
const unsigned int kRequiredOutputPortCaps =
    SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;

const unsigned int kCreateOutputPortCaps =
    SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_NO_EXPORT;
const unsigned int kCreateInputPortCaps =
    SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_NO_EXPORT;
const unsigned int kCreatePortType =
    SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION;

int AddrToInt(int client, int port) {
  return (client << 8) | port;
}

void SetStringIfNonEmpty(base::DictionaryValue* value,
                         const std::string& path,
                         const std::string& in_value) {
  if (!in_value.empty())
    value->SetString(path, in_value);
}

}  // namespace

MidiManagerAlsa::MidiManagerAlsa()
    : in_client_(NULL),
      out_client_(NULL),
      out_client_id_(-1),
      in_port_id_(-1),
      decoder_(NULL),
      udev_(device::udev_new()),
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
  in_client_id_ = snd_seq_client_id(in_client_);
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
  in_port_id_ = snd_seq_create_simple_port(
      in_client_, NULL, kCreateInputPortCaps, kCreatePortType);
  if (in_port_id_ < 0) {
    VLOG(1) << "snd_seq_create_simple_port fails: "
            << snd_strerror(in_port_id_);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }

  // Subscribe to the announce port.
  snd_seq_port_subscribe_t* subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_addr_t announce_sender;
  snd_seq_addr_t announce_dest;
  announce_sender.client = SND_SEQ_CLIENT_SYSTEM;
  announce_sender.port = SND_SEQ_PORT_SYSTEM_ANNOUNCE;
  announce_dest.client = in_client_id_;
  announce_dest.port = in_port_id_;
  snd_seq_port_subscribe_set_sender(subs, &announce_sender);
  snd_seq_port_subscribe_set_dest(subs, &announce_dest);
  err = snd_seq_subscribe_port(in_client_, subs);
  if (err != 0) {
    VLOG(1) << "snd_seq_subscribe_port on the announce port fails: "
            << snd_strerror(err);
    return CompleteInitialization(MIDI_INITIALIZATION_ERROR);
  }

  // Generate hotplug events for existing ports.
  EnumerateAlsaPorts();

  // Start processing events.
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

MidiManagerAlsa::MidiPort::MidiPort(const std::string& path,
                                    const std::string& id,
                                    int client_id,
                                    int port_id,
                                    int midi_device,
                                    const std::string& client_name,
                                    const std::string& port_name,
                                    const std::string& manufacturer,
                                    const std::string& version,
                                    Type type)
    : id_(id),
      midi_device_(midi_device),
      type_(type),
      path_(path),
      client_id_(client_id),
      port_id_(port_id),
      client_name_(client_name),
      port_name_(port_name),
      manufacturer_(manufacturer),
      version_(version),
      web_port_index_(0),
      connected_(true) {
}

MidiManagerAlsa::MidiPort::~MidiPort() {
}

// Note: keep synchronized with the MidiPort::Match* methods.
scoped_ptr<base::Value> MidiManagerAlsa::MidiPort::Value() const {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue);

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
  SetStringIfNonEmpty(value.get(), "path", path_);
  SetStringIfNonEmpty(value.get(), "id", id_);
  SetStringIfNonEmpty(value.get(), "clientName", client_name_);
  SetStringIfNonEmpty(value.get(), "portName", port_name_);
  value->SetInteger("clientId", client_id_);
  value->SetInteger("portId", port_id_);
  value->SetInteger("midiDevice", midi_device_);

  return value.Pass();
}

std::string MidiManagerAlsa::MidiPort::JSONValue() const {
  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.Serialize(*Value().get());
  return json;
}

// TODO(agoode): Do not use SHA256 here. Instead store a persistent
//               mapping and just use a UUID or other random string.
//               http://crbug.com/465320
std::string MidiManagerAlsa::MidiPort::OpaqueKey() const {
  uint8 hash[crypto::kSHA256Length];
  crypto::SHA256HashString(JSONValue(), &hash, sizeof(hash));
  return base::HexEncode(&hash, sizeof(hash));
}

bool MidiManagerAlsa::MidiPort::MatchConnected(const MidiPort& query) const {
  // Matches on:
  // connected == true
  // type
  // path
  // id
  // client_id
  // port_id
  // midi_device
  // client_name
  // port_name
  return connected() && (type() == query.type()) && (path() == query.path()) &&
         (id() == query.id()) && (client_id() == query.client_id()) &&
         (port_id() == query.port_id()) &&
         (midi_device() == query.midi_device()) &&
         (client_name() == query.client_name()) &&
         (port_name() == query.port_name());
}

bool MidiManagerAlsa::MidiPort::MatchCardPass1(const MidiPort& query) const {
  // Matches on:
  // connected == false
  // type
  // path
  // id
  // port_id
  // midi_device
  return MatchCardPass2(query) && (path() == query.path());
}

bool MidiManagerAlsa::MidiPort::MatchCardPass2(const MidiPort& query) const {
  // Matches on:
  // connected == false
  // type
  // id
  // port_id
  // midi_device
  return !connected() && (type() == query.type()) && (id() == query.id()) &&
         (port_id() == query.port_id()) &&
         (midi_device() == query.midi_device());
}

bool MidiManagerAlsa::MidiPort::MatchNoCardPass1(const MidiPort& query) const {
  // Matches on:
  // connected == false
  // type
  // path.empty(), for both this and query
  // id.empty(), for both this and query
  // client_id
  // port_id
  // client_name
  // port_name
  // midi_device == -1, for both this and query
  return MatchNoCardPass2(query) && (client_id() == query.client_id());
}

bool MidiManagerAlsa::MidiPort::MatchNoCardPass2(const MidiPort& query) const {
  // Matches on:
  // connected == false
  // type
  // path.empty(), for both this and query
  // id.empty(), for both this and query
  // port_id
  // client_name
  // port_name
  // midi_device == -1, for both this and query
  return !connected() && (type() == query.type()) && path().empty() &&
         query.path().empty() && id().empty() && query.id().empty() &&
         (port_id() == query.port_id()) &&
         (client_name() == query.client_name()) &&
         (port_name() == query.port_name()) && (midi_device() == -1) &&
         (query.midi_device() == -1);
}

MidiManagerAlsa::MidiPortStateBase::~MidiPortStateBase() {
}

ScopedVector<MidiManagerAlsa::MidiPort>*
MidiManagerAlsa::MidiPortStateBase::ports() {
  return &ports_;
}

MidiManagerAlsa::MidiPortStateBase::iterator
MidiManagerAlsa::MidiPortStateBase::Find(
    const MidiManagerAlsa::MidiPort& port) {
  auto result = FindConnected(port);
  if (result == end())
    result = FindDisconnected(port);
  return result;
}

MidiManagerAlsa::MidiPortStateBase::iterator
MidiManagerAlsa::MidiPortStateBase::FindConnected(
    const MidiManagerAlsa::MidiPort& port) {
  // Exact match required for connected ports.
  auto it = std::find_if(ports_.begin(), ports_.end(), [&port](MidiPort* p) {
    return p->MatchConnected(port);
  });
  return it;
}

MidiManagerAlsa::MidiPortStateBase::iterator
MidiManagerAlsa::MidiPortStateBase::FindDisconnected(
    const MidiManagerAlsa::MidiPort& port) {
  // Always match on:
  //  type
  // Possible things to match on:
  //  path
  //  id
  //  client_id
  //  port_id
  //  midi_device
  //  client_name
  //  port_name

  if (!port.path().empty()) {
    // If path is present, then we have a card-based client.

    // Pass 1. Match on path, id, midi_device, port_id.
    // This is the best possible match for hardware card-based clients.
    // This will also match the empty id correctly for devices without an id.
    auto it = std::find_if(ports_.begin(), ports_.end(), [&port](MidiPort* p) {
      return p->MatchCardPass1(port);
    });
    if (it != ports_.end())
      return it;

    if (!port.id().empty()) {
      // Pass 2. Match on id, midi_device, port_id.
      // This will give us a high-confidence match when a user moves a device to
      // another USB/Firewire/Thunderbolt/etc port, but only works if the device
      // has a hardware id.
      it = std::find_if(ports_.begin(), ports_.end(), [&port](MidiPort* p) {
        return p->MatchCardPass2(port);
      });
      if (it != ports_.end())
        return it;
    }
  } else {
    // Else, we have a non-card-based client.
    // Pass 1. Match on client_id, port_id, client_name, port_name.
    // This will give us a reasonably good match.
    auto it = std::find_if(ports_.begin(), ports_.end(), [&port](MidiPort* p) {
      return p->MatchNoCardPass1(port);
    });
    if (it != ports_.end())
      return it;

    // Pass 2. Match on port_id, client_name, port_name.
    // This is weaker but similar to pass 2 in the hardware card-based clients
    // match.
    it = std::find_if(ports_.begin(), ports_.end(), [&port](MidiPort* p) {
      return p->MatchNoCardPass2(port);
    });
    if (it != ports_.end())
      return it;
  }

  // No match.
  return ports_.end();
}

MidiManagerAlsa::MidiPortStateBase::MidiPortStateBase() {
}

void MidiManagerAlsa::TemporaryMidiPortState::Insert(
    scoped_ptr<MidiPort> port) {
  ports()->push_back(port.Pass());
}

MidiManagerAlsa::MidiPortState::MidiPortState()
    : num_input_ports_(0), num_output_ports_(0) {
}

uint32 MidiManagerAlsa::MidiPortState::Insert(scoped_ptr<MidiPort> port) {
  // Add the web midi index.
  uint32 web_port_index = 0;
  switch (port->type()) {
    case MidiPort::Type::kInput:
      web_port_index = num_input_ports_++;
      break;
    case MidiPort::Type::kOutput:
      web_port_index = num_output_ports_++;
      break;
  }
  port->set_web_port_index(web_port_index);
  ports()->push_back(port.Pass());
  return web_port_index;
}

MidiManagerAlsa::AlsaSeqState::AlsaSeqState() : clients_deleter_(&clients_) {
}

MidiManagerAlsa::AlsaSeqState::~AlsaSeqState() {
}

void MidiManagerAlsa::AlsaSeqState::ClientStart(int client_id,
                                                const std::string& client_name,
                                                snd_seq_client_type_t type) {
  ClientExit(client_id);
  clients_[client_id] = new Client(client_name, type);
}

bool MidiManagerAlsa::AlsaSeqState::ClientStarted(int client_id) {
  return clients_.find(client_id) != clients_.end();
}

void MidiManagerAlsa::AlsaSeqState::ClientExit(int client_id) {
  auto it = clients_.find(client_id);
  if (it != clients_.end()) {
    delete it->second;
    clients_.erase(it);
  }
}

void MidiManagerAlsa::AlsaSeqState::PortStart(
    int client_id,
    int port_id,
    const std::string& port_name,
    MidiManagerAlsa::AlsaSeqState::PortDirection direction,
    bool midi) {
  auto it = clients_.find(client_id);
  if (it != clients_.end())
    it->second->AddPort(port_id,
                        scoped_ptr<Port>(new Port(port_name, direction, midi)));
}

void MidiManagerAlsa::AlsaSeqState::PortExit(int client_id, int port_id) {
  auto it = clients_.find(client_id);
  if (it != clients_.end())
    it->second->RemovePort(port_id);
}

snd_seq_client_type_t MidiManagerAlsa::AlsaSeqState::ClientType(
    int client_id) const {
  auto it = clients_.find(client_id);
  if (it == clients_.end())
    return SND_SEQ_USER_CLIENT;
  return it->second->type();
}

scoped_ptr<MidiManagerAlsa::TemporaryMidiPortState>
MidiManagerAlsa::AlsaSeqState::ToMidiPortState() {
  scoped_ptr<MidiManagerAlsa::TemporaryMidiPortState> midi_ports(
      new TemporaryMidiPortState);
  // TODO(agoode): Use information from udev as well.

  for (const auto& client_pair : clients_) {
    int client_id = client_pair.first;
    const auto& client = client_pair.second;

    // Get client metadata.
    const std::string client_name = client->name();
    std::string manufacturer;
    std::string driver;
    std::string path;
    std::string id;
    std::string serial;
    std::string card_name;
    std::string card_longname;
    int midi_device = -1;

    for (const auto& port_pair : *client) {
      int port_id = port_pair.first;
      const auto& port = port_pair.second;

      if (port->midi()) {
        std::string version;
        if (!driver.empty()) {
          version = driver + " / ";
        }
        version +=
            base::StringPrintf("ALSA library version %d.%d.%d", SND_LIB_MAJOR,
                               SND_LIB_MINOR, SND_LIB_SUBMINOR);
        PortDirection direction = port->direction();
        if (direction == PortDirection::kInput ||
            direction == PortDirection::kDuplex) {
          midi_ports->Insert(scoped_ptr<MidiPort>(new MidiPort(
              path, id, client_id, port_id, midi_device, client->name(),
              port->name(), manufacturer, version, MidiPort::Type::kInput)));
        }
        if (direction == PortDirection::kOutput ||
            direction == PortDirection::kDuplex) {
          midi_ports->Insert(scoped_ptr<MidiPort>(new MidiPort(
              path, id, client_id, port_id, midi_device, client->name(),
              port->name(), manufacturer, version, MidiPort::Type::kOutput)));
        }
      }
    }
  }

  return midi_ports.Pass();
}

MidiManagerAlsa::AlsaSeqState::Port::Port(
    const std::string& name,
    MidiManagerAlsa::AlsaSeqState::PortDirection direction,
    bool midi)
    : name_(name), direction_(direction), midi_(midi) {
}

MidiManagerAlsa::AlsaSeqState::Port::~Port() {
}

std::string MidiManagerAlsa::AlsaSeqState::Port::name() const {
  return name_;
}

MidiManagerAlsa::AlsaSeqState::PortDirection
MidiManagerAlsa::AlsaSeqState::Port::direction() const {
  return direction_;
}

bool MidiManagerAlsa::AlsaSeqState::Port::midi() const {
  return midi_;
}

MidiManagerAlsa::AlsaSeqState::Client::Client(const std::string& name,
                                              snd_seq_client_type_t type)
    : name_(name), type_(type), ports_deleter_(&ports_) {
}

MidiManagerAlsa::AlsaSeqState::Client::~Client() {
}

std::string MidiManagerAlsa::AlsaSeqState::Client::name() const {
  return name_;
}

snd_seq_client_type_t MidiManagerAlsa::AlsaSeqState::Client::type() const {
  return type_;
}

void MidiManagerAlsa::AlsaSeqState::Client::AddPort(int addr,
                                                    scoped_ptr<Port> port) {
  RemovePort(addr);
  ports_[addr] = port.release();
}

void MidiManagerAlsa::AlsaSeqState::Client::RemovePort(int addr) {
  auto it = ports_.find(addr);
  if (it != ports_.end()) {
    delete it->second;
    ports_.erase(it);
  }
}

MidiManagerAlsa::AlsaSeqState::Client::PortMap::const_iterator
MidiManagerAlsa::AlsaSeqState::Client::begin() const {
  return ports_.begin();
}

MidiManagerAlsa::AlsaSeqState::Client::PortMap::const_iterator
MidiManagerAlsa::AlsaSeqState::Client::end() const {
  return ports_.end();
}

// static
std::string MidiManagerAlsa::ExtractManufacturerString(
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

  // Is the vendor string present and not just the vendor hex id?
  if (!udev_id_vendor.empty() && (udev_id_vendor != udev_id_vendor_id)) {
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
      base::AutoLock lock(out_ports_lock_);
      auto it = out_ports_.find(port_index);
      if (it != out_ports_.end()) {
        snd_seq_ev_set_source(&event, it->second);
        snd_seq_ev_set_subs(&event);
        snd_seq_ev_set_direct(&event);
        snd_seq_event_output_direct(out_client_, &event);
      }
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
      case SND_SEQ_EVENT_PORT_START:
        // Don't use SND_SEQ_EVENT_CLIENT_START because the client name may not
        // be set by the time we query it. It should be set by the time ports
        // are made.
        ProcessClientStartEvent(event->data.addr.client);
        ProcessPortStartEvent(event->data.addr);
        break;

      case SND_SEQ_EVENT_CLIENT_EXIT:
        // Check for disconnection of our "out" client. This means "shut down".
        if (event->data.addr.client == out_client_id_)
          return;

        ProcessClientExitEvent(event->data.addr);
        break;

      case SND_SEQ_EVENT_PORT_EXIT:
        ProcessPortExitEvent(event->data.addr);
        break;
    }
  } else {
    ProcessSingleEvent(event, timestamp);
  }

  // Do again.
  ScheduleEventLoop();
}

void MidiManagerAlsa::ProcessSingleEvent(snd_seq_event_t* event,
                                         double timestamp) {
  auto source_it =
      source_map_.find(AddrToInt(event->source.client, event->source.port));
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

void MidiManagerAlsa::ProcessClientStartEvent(int client_id) {
  // Ignore if client is already started.
  if (alsa_seq_state_.ClientStarted(client_id))
    return;

  snd_seq_client_info_t* client_info;
  snd_seq_client_info_alloca(&client_info);
  int err = snd_seq_get_any_client_info(in_client_, client_id, client_info);
  if (err != 0)
    return;

  // Skip our own clients.
  if ((client_id == in_client_id_) || (client_id == out_client_id_))
    return;

  // Update our view of ALSA seq state.
  alsa_seq_state_.ClientStart(client_id,
                              snd_seq_client_info_get_name(client_info),
                              snd_seq_client_info_get_type(client_info));

  // Generate Web MIDI events.
  UpdatePortStateAndGenerateEvents();
}

void MidiManagerAlsa::ProcessPortStartEvent(const snd_seq_addr_t& addr) {
  snd_seq_port_info_t* port_info;
  snd_seq_port_info_alloca(&port_info);
  int err =
      snd_seq_get_any_port_info(in_client_, addr.client, addr.port, port_info);
  if (err != 0)
    return;

  unsigned int caps = snd_seq_port_info_get_capability(port_info);
  bool input = (caps & kRequiredInputPortCaps) == kRequiredInputPortCaps;
  bool output = (caps & kRequiredOutputPortCaps) == kRequiredOutputPortCaps;
  AlsaSeqState::PortDirection direction;
  if (input && output)
    direction = AlsaSeqState::PortDirection::kDuplex;
  else if (input)
    direction = AlsaSeqState::PortDirection::kInput;
  else if (output)
    direction = AlsaSeqState::PortDirection::kOutput;
  else
    return;

  // Update our view of ALSA seq state.
  alsa_seq_state_.PortStart(
      addr.client, addr.port, snd_seq_port_info_get_name(port_info), direction,
      snd_seq_port_info_get_type(port_info) & SND_SEQ_PORT_TYPE_MIDI_GENERIC);
  // Generate Web MIDI events.
  UpdatePortStateAndGenerateEvents();
}

void MidiManagerAlsa::ProcessClientExitEvent(const snd_seq_addr_t& addr) {
  // Update our view of ALSA seq state.
  alsa_seq_state_.ClientExit(addr.client);
  // Generate Web MIDI events.
  UpdatePortStateAndGenerateEvents();
}

void MidiManagerAlsa::ProcessPortExitEvent(const snd_seq_addr_t& addr) {
  // Update our view of ALSA seq state.
  alsa_seq_state_.PortExit(addr.client, addr.port);
  // Generate Web MIDI events.
  UpdatePortStateAndGenerateEvents();
}

void MidiManagerAlsa::UpdatePortStateAndGenerateEvents() {
  // Generate new port state.
  auto new_port_state = alsa_seq_state_.ToMidiPortState();

  // Disconnect any connected old ports that are now missing.
  for (auto* old_port : port_state_) {
    if (old_port->connected() &&
        (new_port_state->FindConnected(*old_port) == new_port_state->end())) {
      old_port->set_connected(false);
      uint32 web_port_index = old_port->web_port_index();
      switch (old_port->type()) {
        case MidiPort::Type::kInput:
          source_map_.erase(
              AddrToInt(old_port->client_id(), old_port->port_id()));
          SetInputPortState(web_port_index, MIDI_PORT_DISCONNECTED);
          break;
        case MidiPort::Type::kOutput:
          DeleteAlsaOutputPort(web_port_index);
          SetOutputPortState(web_port_index, MIDI_PORT_DISCONNECTED);
          break;
      }
    }
  }

  // Reconnect or add new ports.
  auto it = new_port_state->begin();
  while (it != new_port_state->end()) {
    auto* new_port = *it;
    auto old_port = port_state_.Find(*new_port);
    if (old_port == port_state_.end()) {
      // Add new port.
      uint32 web_port_index =
          port_state_.Insert(scoped_ptr<MidiPort>(new_port));
      MidiPortInfo info(new_port->OpaqueKey(), new_port->manufacturer(),
                        new_port->port_name(), new_port->version(),
                        MIDI_PORT_OPENED);
      switch (new_port->type()) {
        case MidiPort::Type::kInput:
          if (Subscribe(web_port_index, new_port->client_id(),
                        new_port->port_id()))
            AddInputPort(info);
          break;

        case MidiPort::Type::kOutput:
          if (CreateAlsaOutputPort(web_port_index, new_port->client_id(),
                                   new_port->port_id()))
            AddOutputPort(info);
          break;
      }
      it = new_port_state->weak_erase(it);
    } else if (!(*old_port)->connected()) {
      // Reconnect.
      uint32 web_port_index = (*old_port)->web_port_index();
      (*old_port)->Update(new_port->path(), new_port->client_id(),
                          new_port->port_id(), new_port->client_name(),
                          new_port->port_name(), new_port->manufacturer(),
                          new_port->version());
      switch ((*old_port)->type()) {
        case MidiPort::Type::kInput:
          if (Subscribe(web_port_index, (*old_port)->client_id(),
                        (*old_port)->port_id()))
            SetInputPortState(web_port_index, MIDI_PORT_OPENED);
          break;

        case MidiPort::Type::kOutput:
          if (CreateAlsaOutputPort(web_port_index, (*old_port)->client_id(),
                                   (*old_port)->port_id()))
            SetOutputPortState(web_port_index, MIDI_PORT_OPENED);
          break;
      }
      (*old_port)->set_connected(true);
      ++it;
    } else {
      ++it;
    }
  }
}

void MidiManagerAlsa::EnumerateAlsaPorts() {
  snd_seq_client_info_t* client_info;
  snd_seq_client_info_alloca(&client_info);
  snd_seq_port_info_t* port_info;
  snd_seq_port_info_alloca(&port_info);

  // Enumerate clients.
  snd_seq_client_info_set_client(client_info, -1);
  while (!snd_seq_query_next_client(in_client_, client_info)) {
    int client_id = snd_seq_client_info_get_client(client_info);
    ProcessClientStartEvent(client_id);

    // Enumerate ports.
    snd_seq_port_info_set_client(port_info, client_id);
    snd_seq_port_info_set_port(port_info, -1);
    while (!snd_seq_query_next_port(in_client_, port_info)) {
      const snd_seq_addr_t* addr = snd_seq_port_info_get_addr(port_info);
      ProcessPortStartEvent(*addr);
    }
  }
}

bool MidiManagerAlsa::CreateAlsaOutputPort(uint32 port_index,
                                           int client_id,
                                           int port_id) {
  // Create the port.
  int out_port = snd_seq_create_simple_port(
      out_client_, NULL, kCreateOutputPortCaps, kCreatePortType);
  if (out_port < 0) {
    VLOG(1) << "snd_seq_create_simple_port fails: " << snd_strerror(out_port);
    return false;
  }
  // Activate port subscription.
  snd_seq_port_subscribe_t* subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_addr_t sender;
  sender.client = out_client_id_;
  sender.port = out_port;
  snd_seq_port_subscribe_set_sender(subs, &sender);
  snd_seq_addr_t dest;
  dest.client = client_id;
  dest.port = port_id;
  snd_seq_port_subscribe_set_dest(subs, &dest);
  int err = snd_seq_subscribe_port(out_client_, subs);
  if (err != 0) {
    VLOG(1) << "snd_seq_subscribe_port fails: " << snd_strerror(err);
    snd_seq_delete_simple_port(out_client_, out_port);
    return false;
  }

  // Update our map.
  base::AutoLock lock(out_ports_lock_);
  out_ports_[port_index] = out_port;
  return true;
}

void MidiManagerAlsa::DeleteAlsaOutputPort(uint32 port_index) {
  base::AutoLock lock(out_ports_lock_);
  auto it = out_ports_.find(port_index);
  if (it == out_ports_.end())
    return;

  int alsa_port = it->second;
  snd_seq_delete_simple_port(out_client_, alsa_port);
  out_ports_.erase(it);
}

bool MidiManagerAlsa::Subscribe(uint32 port_index, int client_id, int port_id) {
  // Activate port subscription.
  snd_seq_port_subscribe_t* subs;
  snd_seq_port_subscribe_alloca(&subs);
  snd_seq_addr_t sender;
  sender.client = client_id;
  sender.port = port_id;
  snd_seq_port_subscribe_set_sender(subs, &sender);
  snd_seq_addr_t dest;
  dest.client = in_client_id_;
  dest.port = in_port_id_;
  snd_seq_port_subscribe_set_dest(subs, &dest);
  int err = snd_seq_subscribe_port(in_client_, subs);
  if (err != 0) {
    VLOG(1) << "snd_seq_subscribe_port fails: " << snd_strerror(err);
    return false;
  }

  // Update our map.
  source_map_[AddrToInt(client_id, port_id)] = port_index;
  return true;
}

MidiManager* MidiManager::Create() {
  return new MidiManagerAlsa();
}

}  // namespace midi
}  // namespace media
