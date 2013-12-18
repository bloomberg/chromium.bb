// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_manager_linux.h"

#include <fcntl.h>
#include <sound/asound.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "media/midi/midi_port_info.h"

namespace media {
namespace {

const char kDevSnd[] = "/dev/snd";
const char kMIDIPattern[] = "midi*";
const char kUnknown[] = "[unknown]";

}  // namespace

class MIDIManagerLinux::MIDIDeviceInfo
    : public base::RefCounted<MIDIDeviceInfo> {
 public:
  MIDIDeviceInfo(MIDIManagerLinux* manager, base::FilePath path)
      : flags_(0) {
    file_ = base::File(path,
                       base::File::FLAG_OPEN |
                       base::File::FLAG_READ |
                       base::File::FLAG_WRITE);
    if (file_.error() != base::File::FILE_OK)
      return;

    struct snd_rawmidi_info info = {};
    if (ioctl(file_.GetPlatformFile(), SNDRV_RAWMIDI_IOCTL_INFO, &info))
      return;

    flags_ = info.flags;
    const char* name = reinterpret_cast<const char*>(&info.name[0]);
    port_info_ = MIDIPortInfo(path.value(), kUnknown, name, kUnknown);
  }

  void Send(MIDIManagerClient* client, const std::vector<uint8>& data) {
    if (!file_.IsValid())
      return;
    file_.WriteAtCurrentPos(reinterpret_cast<const char*>(&data[0]),
                            data.size());
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MIDIManagerClient::AccumulateMIDIBytesSent,
                   base::Unretained(client), data.size()));
  }

  bool HasInputPort() const { return flags_ & SNDRV_RAWMIDI_INFO_INPUT; }
  bool HasOutputPort() const { return flags_ & SNDRV_RAWMIDI_INFO_OUTPUT; }
  const MIDIPortInfo& GetMIDIPortInfo() const { return port_info_; }

 private:
  friend class base::RefCounted<MIDIDeviceInfo>;
  virtual ~MIDIDeviceInfo() {}

  base::File file_;
  int flags_;
  MIDIPortInfo port_info_;

  DISALLOW_COPY_AND_ASSIGN(MIDIDeviceInfo);
};

MIDIManagerLinux::MIDIManagerLinux()
    : send_thread_("MIDISendThread") {
}

bool MIDIManagerLinux::Initialize() {
  TRACE_EVENT0("midi", "MIDIManagerMac::Initialize");

  base::FilePath path(kDevSnd);
  base::FileEnumerator enumerator(path, false, base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL(kMIDIPattern));
  for (base::FilePath name = enumerator.Next();
       !name.empty();
       name = enumerator.Next()) {
    scoped_refptr<MIDIDeviceInfo> device = new MIDIDeviceInfo(this, name);
    if (device->HasInputPort()) {
      in_devices_.push_back(device);
      AddInputPort(device->GetMIDIPortInfo());
    }
    if (device->HasOutputPort()) {
      out_devices_.push_back(device);
      AddOutputPort(device->GetMIDIPortInfo());
    }
  }
  return true;
}

MIDIManagerLinux::~MIDIManagerLinux() {
  send_thread_.Stop();
}

void MIDIManagerLinux::DispatchSendMIDIData(MIDIManagerClient* client,
                                            uint32 port_index,
                                            const std::vector<uint8>& data,
                                            double timestamp) {
  if (out_devices_.size() <= port_index)
    return;

  base::TimeDelta delay;
  if (timestamp != 0.0) {
    base::TimeTicks time_to_send =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(
            timestamp * base::Time::kMicrosecondsPerSecond);
    delay = std::max(time_to_send - base::TimeTicks::Now(), base::TimeDelta());
  }

  if (!send_thread_.IsRunning())
    send_thread_.Start();

  scoped_refptr<MIDIDeviceInfo> device = out_devices_[port_index];
  send_thread_.message_loop()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MIDIDeviceInfo::Send, device, client, data),
      delay);
}

MIDIManager* MIDIManager::Create() {
  return new MIDIManagerLinux();
}

}  // namespace media
