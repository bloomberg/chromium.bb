// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MIDI_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MIDI_HOST_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "media/midi/midi_manager.h"

namespace media {
class MIDIManager;
}

namespace content {

class CONTENT_EXPORT MIDIHost
    : public BrowserMessageFilter,
      public media::MIDIManagerClient {
 public:
  // Called from UI thread from the owner of this object.
  MIDIHost(media::MIDIManager* midi_manager);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // MIDIManagerClient implementation.
  virtual void ReceiveMIDIData(
      int port_index,
      const uint8* data,
      size_t length,
      double timestamp) OVERRIDE;

  // Request access to MIDI hardware.
  void OnRequestAccess(int client_id, int access);

  // Data to be sent to a MIDI output port.
  void OnSendData(int port,
                  const std::vector<uint8>& data,
                  double timestamp);

 private:
  friend class base::DeleteHelper<MIDIHost>;
  friend class BrowserThread;

  virtual ~MIDIHost();

  media::MIDIManager* const midi_manager_;

  DISALLOW_COPY_AND_ASSIGN(MIDIHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MIDI_HOST_H_
