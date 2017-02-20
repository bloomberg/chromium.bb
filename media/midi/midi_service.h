// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_SERVICE_H_
#define MEDIA_MIDI_MIDI_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "media/midi/midi_export.h"
#include "media/midi/midi_manager.h"

namespace midi {

// Manages MidiManager backends.  This class expects to be constructed and
// destructed on the browser main thread, but methods can be called on both
// the main thread and the I/O thread.
class MIDI_EXPORT MidiService final {
 public:
  // Use the first constructor for production code.
  MidiService();
  // |MidiManager| can be explicitly specified in the constructor for testing.
  explicit MidiService(std::unique_ptr<MidiManager> manager);
  ~MidiService();

  // Called on the browser main thread to notify the I/O thread will stop and
  // the instance will be destructed on the main thread soon.
  void Shutdown();

  // A client calls StartSession() to receive and send MIDI data.
  void StartSession(MidiManagerClient* client);

  // A client calls EndSession() to stop receiving MIDI data.
  void EndSession(MidiManagerClient* client);

  // A client calls DispatchSendMidiData() to send MIDI data.
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            double timestamp);

  // Returns a SingleThreadTaskRunner reference to serve MidiManager. Each
  // TaskRunner will be constructed on demand.
  // MidiManager that supports the dynamic instantiation feature will use this
  // method to post tasks that should not run on I/O. Since TaskRunners outlive
  // MidiManager, each task should ensure that MidiManager that posted the task
  // is still alive while accessing |this|. TaskRunners will be reused when
  // another MidiManager is instantiated.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(size_t runner_id);

 private:
  // Holds MidiManager instance. If the dynamic instantiation feature is
  // enabled, the MidiManager would be constructed and destructed on the I/O
  // thread, and all MidiManager methods would be called on the I/O thread.
  std::unique_ptr<MidiManager> manager_;

  // A flag to indicate if the dynamic instantiation feature is supported and
  // actually enabled.
  const bool is_dynamic_instantiation_enabled_;

  // Counts active clients to manage dynamic MidiManager instantiation.
  size_t active_clients_;

  // Protects all members above.
  base::Lock lock_;

  // Threads to host SingleThreadTaskRunners.
  std::vector<std::unique_ptr<base::Thread>> threads_;

  // Protects |threads_|.
  base::Lock threads_lock_;

  DISALLOW_COPY_AND_ASSIGN(MidiService);
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_SERVICE_H_
