// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_key_event_listener.h"

#include "third_party/cros/chromeos_wm_ipc_enums.h"

namespace chromeos {

// For now, this file contains the SystemKeyEventListener, which listens for key
// presses from Window Manager, and just calls amixer to adjust volume.  Start
// by just calling instance() to get it going.
//
// TODO(davej): Create AudioHandler() class and call its volume up/down/mute
// functions, getting rid of these constants and RunCommand().  This class will
// eventually call PulseAudio directly to get current volume and adjust
// accordingly, giving us more control over Mute/UnMute behavior as well.

namespace {

const char kIncreaseVolumeLevelCommand[] =
    "/usr/bin/amixer -- sset Master unmute 5%+";
const char kDecreaseVolumeLevelCommand[] =
    "/usr/bin/amixer -- sset Master unmute 5%-";
const char kMuteAudioCommand[] =
    "/usr/bin/amixer -- sset Master mute";

}  // namespace

// static
SystemKeyEventListener* SystemKeyEventListener::instance() {
  SystemKeyEventListener* instance = Singleton<SystemKeyEventListener>::get();
  return instance;
}

SystemKeyEventListener::SystemKeyEventListener() {
  WmMessageListener::instance()->AddObserver(this);
}

SystemKeyEventListener::~SystemKeyEventListener() {
  WmMessageListener::instance()->RemoveObserver(this);
}

void SystemKeyEventListener::ProcessWmMessage(const WmIpc::Message& message,
                                              GdkWindow* window) {
  if (message.type() != WM_IPC_MESSAGE_CHROME_NOTIFY_SYSKEY_PRESSED)
    return;

  // TODO(davej): Use WmIpcSystemKey enums when available.
  switch (message.param(0)) {
    case 0:
      RunCommand(kMuteAudioCommand);
      break;
    case 1:
      RunCommand(kDecreaseVolumeLevelCommand);
      break;
    case 2:
      RunCommand(kIncreaseVolumeLevelCommand);
      break;
    default:
      DLOG(ERROR) << "SystemKeyEventListener: Unexpected message "
                  << message.param(0)
                  << " received";
  }
}

void SystemKeyEventListener::RunCommand(std::string command) {
  if (command.empty())
    return;
  command += " &";
  DLOG(INFO) << "Running command \"" << command << "\"";
  if (system(command.c_str()) < 0)
    LOG(WARNING) << "Got error while running \"" << command << "\"";
}

}  // namespace chromeos
