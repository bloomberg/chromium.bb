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
  virtual void AccumulateMIDIBytesSent(size_t n) OVERRIDE;

  // Start session to access MIDI hardware.
  void OnStartSession(int client_id);

  // Data to be sent to a MIDI output port.
  void OnSendData(int port,
                  const std::vector<uint8>& data,
                  double timestamp);

 private:
  friend class base::DeleteHelper<MIDIHost>;
  friend class BrowserThread;

  virtual ~MIDIHost();

  // |midi_manager_| talks to the platform-specific MIDI APIs.
  // It can be NULL if the platform (or our current implementation)
  // does not support MIDI.  If not supported then a call to
  // OnRequestAccess() will always refuse access and a call to
  // OnSendData() will do nothing.
  media::MIDIManager* const midi_manager_;

  // The number of bytes sent to the platform-specific MIDI sending
  // system, but not yet completed.
  size_t sent_bytes_in_flight_;

  // The number of bytes successfully sent since the last time
  // we've acknowledged back to the renderer.
  size_t bytes_sent_since_last_acknowledgement_;

  // Protects access to |sent_bytes_in_flight_|.
  base::Lock in_flight_lock_;

  DISALLOW_COPY_AND_ASSIGN(MIDIHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MIDI_HOST_H_
