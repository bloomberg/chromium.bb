// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"

namespace media {
namespace midi {

namespace {

using Sample = base::HistogramBase::Sample;

// If many users have more devices, this number will be increased.
// But the number is expected to be big enough for now.
const Sample kMaxUmaDevices = 31;

// Used to count events for usage histogram.
enum class Usage {
  CREATED,
  CREATED_ON_UNSUPPORTED_PLATFORMS,
  SESSION_STARTED,
  SESSION_ENDED,
  INITIALIZED,
  INPUT_PORT_ADDED,
  OUTPUT_PORT_ADDED,

  // New items should be inserted here, and |MAX| should point the last item.
  MAX = INITIALIZED,
};

void ReportUsage(Usage usage) {
  UMA_HISTOGRAM_ENUMERATION("Media.Midi.Usage",
                            static_cast<Sample>(usage),
                            static_cast<Sample>(Usage::MAX) + 1);
}

}  // namespace

MidiManager::MidiManager()
    : initialized_(false), result_(Result::NOT_INITIALIZED) {
  ReportUsage(Usage::CREATED);
}

MidiManager::~MidiManager() {
  UMA_HISTOGRAM_ENUMERATION("Media.Midi.ResultOnShutdown",
                            static_cast<Sample>(result_),
                            static_cast<Sample>(Result::MAX) + 1);
}

#if !defined(OS_MACOSX) && !defined(OS_WIN) && \
    !(defined(USE_ALSA) && defined(USE_UDEV)) && !defined(OS_ANDROID)
MidiManager* MidiManager::Create() {
  ReportUsage(Usage::CREATED_ON_UNSUPPORTED_PLATFORMS);
  return new MidiManager;
}
#endif

void MidiManager::StartSession(MidiManagerClient* client) {
  ReportUsage(Usage::SESSION_STARTED);

  bool session_is_ready;
  bool session_needs_initialization = false;
  bool too_many_pending_clients_exist = false;

  {
    base::AutoLock auto_lock(lock_);
    session_is_ready = initialized_;
    if (clients_.find(client) != clients_.end() ||
        pending_clients_.find(client) != pending_clients_.end()) {
      // Should not happen. But just in case the renderer is compromised.
      NOTREACHED();
      return;
    }
    if (!session_is_ready) {
      // Do not accept a new request if the pending client list contains too
      // many clients.
      too_many_pending_clients_exist =
          pending_clients_.size() >= kMaxPendingClientCount;

      if (!too_many_pending_clients_exist) {
        // Call StartInitialization() only for the first request.
        session_needs_initialization = pending_clients_.empty();
        pending_clients_.insert(client);
      }
    }
  }

  // Lazily initialize the MIDI back-end.
  if (!session_is_ready) {
    if (session_needs_initialization) {
      TRACE_EVENT0("midi", "MidiManager::StartInitialization");
      session_thread_runner_ =
          base::MessageLoop::current()->task_runner();
      StartInitialization();
    }
    if (too_many_pending_clients_exist) {
      // Return an error immediately if there are too many requests.
      client->CompleteStartSession(Result::INITIALIZATION_ERROR);
      return;
    }
    // CompleteInitialization() will be called asynchronously when platform
    // dependent initialization is finished.
    return;
  }

  // Platform dependent initialization was already finished for previously
  // initialized clients.
  Result result;
  {
    base::AutoLock auto_lock(lock_);
    if (result_ == Result::OK) {
      AddInitialPorts(client);
      clients_.insert(client);
    }
    result = result_;
  }
  client->CompleteStartSession(result);
}

void MidiManager::EndSession(MidiManagerClient* client) {
  ReportUsage(Usage::SESSION_ENDED);

  // At this point, |client| can be in the destruction process, and calling
  // any method of |client| is dangerous.
  base::AutoLock auto_lock(lock_);
  clients_.erase(client);
  pending_clients_.erase(client);
}

void MidiManager::AccumulateMidiBytesSent(MidiManagerClient* client, size_t n) {
  {
    base::AutoLock auto_lock(lock_);
    if (clients_.find(client) == clients_.end())
      return;
  }
  client->AccumulateMidiBytesSent(n);
}

void MidiManager::DispatchSendMidiData(MidiManagerClient* client,
                                       uint32 port_index,
                                       const std::vector<uint8>& data,
                                       double timestamp) {
  NOTREACHED();
}

void MidiManager::StartInitialization() {
  CompleteInitialization(Result::NOT_SUPPORTED);
}

void MidiManager::CompleteInitialization(Result result) {
  DCHECK(session_thread_runner_.get());
  // It is safe to post a task to the IO thread from here because the IO thread
  // should have stopped if the MidiManager is going to be destructed.
  session_thread_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MidiManager::CompleteInitializationInternal,
                 base::Unretained(this),
                 result));
}

void MidiManager::AddInputPort(const MidiPortInfo& info) {
  ReportUsage(Usage::INPUT_PORT_ADDED);
  base::AutoLock auto_lock(lock_);
  input_ports_.push_back(info);
  for (auto client : clients_)
    client->AddInputPort(info);
}

void MidiManager::AddOutputPort(const MidiPortInfo& info) {
  ReportUsage(Usage::OUTPUT_PORT_ADDED);
  base::AutoLock auto_lock(lock_);
  output_ports_.push_back(info);
  for (auto client : clients_)
    client->AddOutputPort(info);
}

void MidiManager::SetInputPortState(uint32 port_index, MidiPortState state) {
  base::AutoLock auto_lock(lock_);
  DCHECK_LT(port_index, input_ports_.size());
  input_ports_[port_index].state = state;
  for (auto client : clients_)
    client->SetInputPortState(port_index, state);
}

void MidiManager::SetOutputPortState(uint32 port_index, MidiPortState state) {
  base::AutoLock auto_lock(lock_);
  DCHECK_LT(port_index, output_ports_.size());
  output_ports_[port_index].state = state;
  for (auto client : clients_)
    client->SetOutputPortState(port_index, state);
}

void MidiManager::ReceiveMidiData(
    uint32 port_index,
    const uint8* data,
    size_t length,
    double timestamp) {
  base::AutoLock auto_lock(lock_);

  for (auto client : clients_)
    client->ReceiveMidiData(port_index, data, length, timestamp);
}

void MidiManager::CompleteInitializationInternal(Result result) {
  TRACE_EVENT0("midi", "MidiManager::CompleteInitialization");
  ReportUsage(Usage::INITIALIZED);
  UMA_HISTOGRAM_ENUMERATION("Media.Midi.InputPorts",
                            static_cast<Sample>(input_ports_.size()),
                            kMaxUmaDevices + 1);
  UMA_HISTOGRAM_ENUMERATION("Media.Midi.OutputPorts",
                            static_cast<Sample>(output_ports_.size()),
                            kMaxUmaDevices + 1);

  base::AutoLock auto_lock(lock_);
  DCHECK(clients_.empty());
  DCHECK(!initialized_);
  initialized_ = true;
  result_ = result;

  for (auto client : pending_clients_) {
    if (result_ == Result::OK) {
      AddInitialPorts(client);
      clients_.insert(client);
    }
    client->CompleteStartSession(result_);
  }
  pending_clients_.clear();
}

void MidiManager::AddInitialPorts(MidiManagerClient* client) {
  lock_.AssertAcquired();

  for (const auto& info : input_ports_)
    client->AddInputPort(info);
  for (const auto& info : output_ports_)
    client->AddOutputPort(info);
}

}  // namespace midi
}  // namespace media
