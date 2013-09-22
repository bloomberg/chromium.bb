// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_DEMUXER_ANDROID_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_DEMUXER_ANDROID_H_

#include "base/id_map.h"
#include "content/public/browser/browser_message_filter.h"
#include "media/base/android/demuxer_android.h"

namespace content {

// Represents the browser process half of an IPC-based implementation of
// media::DemuxerAndroid.
//
// Refer to RendererDemuxerAndroid for the renderer process half.
class CONTENT_EXPORT BrowserDemuxerAndroid
    : public BrowserMessageFilter,
      public media::DemuxerAndroid {
 public:
  BrowserDemuxerAndroid();

  // BrowserMessageFilter overrides.
  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // media::DemuxerAndroid implementation.
  virtual void AddDemuxerClient(int demuxer_client_id,
                                media::DemuxerAndroidClient* client) OVERRIDE;
  virtual void RemoveDemuxerClient(int demuxer_client_id) OVERRIDE;
  virtual void RequestDemuxerConfigs(int demuxer_client_id) OVERRIDE;
  virtual void RequestDemuxerData(int demuxer_client_id,
                                  media::DemuxerStream::Type type) OVERRIDE;
  virtual void RequestDemuxerSeek(int demuxer_client_id,
                                  base::TimeDelta time_to_seek,
                                  unsigned seek_request_id) OVERRIDE;

 protected:
  friend class base::RefCountedThreadSafe<BrowserDemuxerAndroid>;
  virtual ~BrowserDemuxerAndroid();

 private:
  void OnDemuxerReady(int demuxer_client_id,
                      const media::DemuxerConfigs& configs);
  void OnReadFromDemuxerAck(int demuxer_client_id,
                            const media::DemuxerData& data);
  void OnMediaSeekRequestAck(int demuxer_client_id, unsigned seek_request_id);
  void OnDurationChanged(int demuxer_client_id,
                         const base::TimeDelta& duration);

  IDMap<media::DemuxerAndroidClient> demuxer_clients_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDemuxerAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_DEMUXER_ANDROID_H_
