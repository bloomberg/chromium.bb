// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_DEMUXER_ANDROID_H_
#define MEDIA_BASE_ANDROID_DEMUXER_ANDROID_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"

namespace media {

class DemuxerAndroidClient;
struct DemuxerConfigs;
struct DemuxerData;

// Defines a demuxer with ID-based asynchronous operations.
//
// TODO(scherkus): Remove |demuxer_client_id| and Add/RemoveDemuxerClient().
// It's required in the interim as the Android Media Source implementation uses
// the MediaPlayerAndroid interface and associated IPC messages.
class MEDIA_EXPORT DemuxerAndroid {
 public:
  // Associates |client| with the demuxer using |demuxer_client_id| as the
  // identifier. Must be called prior to calling any other methods.
  virtual void AddDemuxerClient(int demuxer_client_id,
                                DemuxerAndroidClient* client) = 0;

  // Removes the association created by AddClient(). Must be called when the
  // client no longer wants to receive updates.
  virtual void RemoveDemuxerClient(int demuxer_client_id) = 0;

  // Called to request the current audio/video decoder configurations.
  virtual void RequestDemuxerConfigs(int demuxer_client_id) = 0;

  // Called to request additiona data from the demuxer.
  virtual void RequestDemuxerData(int demuxer_client_id,
                                  media::DemuxerStream::Type type) = 0;

  // Called to request the demuxer to seek to a particular media time.
  virtual void RequestDemuxerSeek(int demuxer_client_id,
                                  base::TimeDelta time_to_seek,
                                  unsigned seek_request_id) = 0;

 protected:
  virtual ~DemuxerAndroid() {}
};

// Defines the client callback interface.
class MEDIA_EXPORT DemuxerAndroidClient {
 public:
  // Called in response to RequestDemuxerConfigs() and also when the demuxer has
  // initialized.
  //
  // TODO(scherkus): Perhaps clients should be required to call
  // RequestDemuxerConfigs() to initialize themselves instead of the demuxer
  // calling this method without being prompted.
  virtual void OnDemuxerConfigsAvailable(const DemuxerConfigs& params) = 0;

  // Called in response to RequestDemuxerData().
  virtual void OnDemuxerDataAvailable(const DemuxerData& params) = 0;

  // Called in response to RequestDemuxerSeek().
  virtual void OnDemuxerSeeked(unsigned seek_request_id) = 0;

  // Called whenever the demuxer has detected a duration change.
  virtual void OnDemuxerDurationChanged(base::TimeDelta duration) = 0;

 protected:
  virtual ~DemuxerAndroidClient() {}
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_DEMUXER_ANDROID_H_
