// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MANAGER_ALSA_H_
#define MEDIA_MIDI_MIDI_MANAGER_ALSA_H_

#include <alsa/asoundlib.h>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "device/udev_linux/scoped_udev.h"
#include "media/midi/midi_export.h"
#include "media/midi/midi_manager.h"

namespace media {
namespace midi {

class MIDI_EXPORT MidiManagerAlsa final : public MidiManager {
 public:
  MidiManagerAlsa();
  ~MidiManagerAlsa() override;

  // MidiManager implementation.
  void StartInitialization() override;
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32 port_index,
                            const std::vector<uint8>& data,
                            double timestamp) override;

 private:
  friend class MidiManagerAlsaTest;
  FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, ExtractManufacturer);
  FRIEND_TEST_ALL_PREFIXES(MidiManagerAlsaTest, ToMidiPortState);

  class MidiPort {
   public:
    enum class Type { kInput, kOutput };

    MidiPort(const std::string& path,
             const std::string& id,
             int client_id,
             int port_id,
             int midi_device,
             const std::string& client_name,
             const std::string& port_name,
             const std::string& manufacturer,
             const std::string& version,
             Type type);
    ~MidiPort();

    // Gets a Value representation of this object, suitable for serialization.
    scoped_ptr<base::Value> Value() const;

    // Gets a string version of Value in JSON format.
    std::string JSONValue() const;

    // Gets an opaque identifier for this object, suitable for using as the id
    // field in MidiPort.id on the web. Note that this string does not store
    // the full state.
    std::string OpaqueKey() const;

    // Checks for equality for connected ports.
    bool MatchConnected(const MidiPort& query) const;
    // Checks for equality for kernel cards with id, pass 1.
    bool MatchCardPass1(const MidiPort& query) const;
    // Checks for equality for kernel cards with id, pass 2.
    bool MatchCardPass2(const MidiPort& query) const;
    // Checks for equality for non-card clients, pass 1.
    bool MatchNoCardPass1(const MidiPort& query) const;
    // Checks for equality for non-card clients, pass 2.
    bool MatchNoCardPass2(const MidiPort& query) const;

    // accessors
    std::string path() const { return path_; }
    std::string id() const { return id_; }
    std::string client_name() const { return client_name_; }
    std::string port_name() const { return port_name_; }
    std::string manufacturer() const { return manufacturer_; }
    std::string version() const { return version_; }
    int client_id() const { return client_id_; }
    int port_id() const { return port_id_; }
    int midi_device() const { return midi_device_; }
    Type type() const { return type_; }
    uint32 web_port_index() const { return web_port_index_; }
    bool connected() const { return connected_; }

    // mutators
    void set_web_port_index(uint32 web_port_index) {
      web_port_index_ = web_port_index;
    }
    void set_connected(bool connected) { connected_ = connected; }
    void Update(const std::string& path,
                int client_id,
                int port_id,
                const std::string& client_name,
                const std::string& port_name,
                const std::string& manufacturer,
                const std::string& version) {
      path_ = path;
      client_id_ = client_id;
      port_id_ = port_id;
      client_name_ = client_name;
      port_name_ = port_name;
      manufacturer_ = manufacturer;
      version_ = version;
    }

   private:
    // Immutable properties.
    const std::string id_;
    const int midi_device_;

    const Type type_;

    // Mutable properties. These will get updated as ports move around or
    // drivers change.
    std::string path_;
    int client_id_;
    int port_id_;
    std::string client_name_;
    std::string port_name_;
    std::string manufacturer_;
    std::string version_;

    // Index for MidiManager.
    uint32 web_port_index_;

    // Port is present in the ALSA system.
    bool connected_;

    DISALLOW_COPY_AND_ASSIGN(MidiPort);
  };

  class MidiPortStateBase {
   public:
    typedef ScopedVector<MidiPort>::iterator iterator;

    virtual ~MidiPortStateBase();

    // Given a port, finds a port in the internal store.
    iterator Find(const MidiPort& port);

    // Given a port, finds a connected port, using exact matching.
    iterator FindConnected(const MidiPort& port);

    // Given a port, finds a disconnected port, using heuristic matching.
    iterator FindDisconnected(const MidiPort& port);

    iterator begin() { return ports_.begin(); }
    iterator end() { return ports_.end(); }

   protected:
    MidiPortStateBase();

    ScopedVector<MidiPort>* ports();

   private:
    ScopedVector<MidiPort> ports_;

    DISALLOW_COPY_AND_ASSIGN(MidiPortStateBase);
  };

  class TemporaryMidiPortState final : public MidiPortStateBase {
   public:
    // Removes a port from the list without deleting it.
    iterator weak_erase(iterator position) {
      return ports()->weak_erase(position);
    }

    void Insert(scoped_ptr<MidiPort> port);
  };

  class MidiPortState final : public MidiPortStateBase {
   public:
    MidiPortState();

    // Inserts a port. Returns web_port_index.
    uint32 Insert(scoped_ptr<MidiPort> port);

   private:
    uint32 num_input_ports_;
    uint32 num_output_ports_;
  };

  class AlsaSeqState {
   public:
    enum class PortDirection { kInput, kOutput, kDuplex };

    AlsaSeqState();
    ~AlsaSeqState();

    void ClientStart(int client_id,
                     const std::string& client_name,
                     snd_seq_client_type_t type);
    bool ClientStarted(int client_id);
    void ClientExit(int client_id);
    void PortStart(int client_id,
                   int port_id,
                   const std::string& port_name,
                   PortDirection direction,
                   bool midi);
    void PortExit(int client_id, int port_id);
    snd_seq_client_type_t ClientType(int client_id) const;
    scoped_ptr<TemporaryMidiPortState> ToMidiPortState();

   private:
    class Port {
     public:
      Port(const std::string& name, PortDirection direction, bool midi);
      ~Port();

      std::string name() const;
      PortDirection direction() const;
      // True if this port is a MIDI port, instead of another kind of ALSA port.
      bool midi() const;

     private:
      const std::string name_;
      const PortDirection direction_;
      const bool midi_;

      DISALLOW_COPY_AND_ASSIGN(Port);
    };

    class Client {
     public:
      typedef std::map<int, Port*> PortMap;

      Client(const std::string& name, snd_seq_client_type_t type);
      ~Client();

      std::string name() const;
      snd_seq_client_type_t type() const;
      void AddPort(int addr, scoped_ptr<Port> port);
      void RemovePort(int addr);
      PortMap::const_iterator begin() const;
      PortMap::const_iterator end() const;

     private:
      const std::string name_;
      const snd_seq_client_type_t type_;
      PortMap ports_;
      STLValueDeleter<PortMap> ports_deleter_;

      DISALLOW_COPY_AND_ASSIGN(Client);
    };

    typedef std::map<int, Client*> ClientMap;

    ClientMap clients_;
    STLValueDeleter<ClientMap> clients_deleter_;

    DISALLOW_COPY_AND_ASSIGN(AlsaSeqState);
  };

  typedef base::hash_map<int, uint32> SourceMap;
  typedef base::hash_map<uint32, int> OutPortMap;

  // Extracts the manufacturer using heuristics and a variety of sources.
  static std::string ExtractManufacturerString(
      const std::string& udev_id_vendor,
      const std::string& udev_id_vendor_id,
      const std::string& udev_id_vendor_from_database,
      const std::string& alsa_name,
      const std::string& alsa_longname);

  // An internal callback that runs on MidiSendThread.
  void SendMidiData(uint32 port_index, const std::vector<uint8>& data);

  void ScheduleEventLoop();
  void EventLoop();
  void ProcessSingleEvent(snd_seq_event_t* event, double timestamp);
  void ProcessClientStartEvent(int client_id);
  void ProcessPortStartEvent(const snd_seq_addr_t& addr);
  void ProcessClientExitEvent(const snd_seq_addr_t& addr);
  void ProcessPortExitEvent(const snd_seq_addr_t& addr);

  // Updates port_state_ and Web MIDI state from alsa_seq_state_.
  void UpdatePortStateAndGenerateEvents();

  // Enumerates ports. Call once after subscribing to the announce port.
  void EnumerateAlsaPorts();
  // Returns true if successful.
  bool CreateAlsaOutputPort(uint32 port_index, int client_id, int port_id);
  void DeleteAlsaOutputPort(uint32 port_index);
  // Returns true if successful.
  bool Subscribe(uint32 port_index, int client_id, int port_id);

  AlsaSeqState alsa_seq_state_;
  MidiPortState port_state_;

  // ALSA seq handles.
  snd_seq_t* in_client_;
  int in_client_id_;
  snd_seq_t* out_client_;
  int out_client_id_;

  // One input port, many output ports.
  int in_port_id_;
  OutPortMap out_ports_;       // guarded by out_ports_lock_
  base::Lock out_ports_lock_;  // guards out_ports_

  // Mapping from ALSA client:port to our index.
  SourceMap source_map_;

  // ALSA event -> MIDI coder.
  snd_midi_event_t* decoder_;

  // udev, for querying hardware devices.
  device::ScopedUdevPtr udev_;

  base::Thread send_thread_;
  base::Thread event_thread_;

  bool event_thread_shutdown_;  // guarded by shutdown_lock_
  base::Lock shutdown_lock_;    // guards event_thread_shutdown_

  DISALLOW_COPY_AND_ASSIGN(MidiManagerAlsa);
};

}  // namespace midi
}  // namespace media

#endif  // MEDIA_MIDI_MIDI_MANAGER_ALSA_H_
