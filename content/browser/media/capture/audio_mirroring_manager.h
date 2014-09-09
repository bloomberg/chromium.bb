// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioMirroringManager is a singleton object that maintains a set of active
// audio mirroring destinations and auto-connects/disconnects audio streams
// to/from those destinations.  It is meant to be used exclusively on the IO
// thread.
//
// How it works:
//
//   1. AudioRendererHost gets a CreateStream message from the render process
//      and, among other things, creates an AudioOutputController to control the
//      audio data flow between the render and browser processes.  More
//      importantly, it registers the AudioOutputController with
//      AudioMirroringManager (as a Diverter).
//   2. A user request to mirror all the audio for a WebContents is made.  A
//      MirroringDestination is created, and StartMirroring() is called to begin
//      the mirroring session.  The MirroringDestination is queried to determine
//      which of all the known Diverters will re-route their audio to it.
//   3a. During a mirroring session, AudioMirroringManager may encounter new
//       Diverters, and will query all the MirroringDestinations to determine
//       which is a match, if any.
//   3b. During a mirroring session, a call to StartMirroring() can be made to
//       request a "refresh" query on a MirroringDestination, and this will
//       result in AudioMirroringManager starting/stopping only those Diverters
//       that are not correctly routed to the destination.
//   3c. When a mirroring session is stopped, the remaining destinations will be
//       queried to determine whether diverting should continue to a different
//       destination.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_AUDIO_MIRRORING_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_AUDIO_MIRRORING_MANAGER_H_

#include <set>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/audio/audio_source_diverter.h"

namespace media {
class AudioOutputStream;
}

namespace content {

class CONTENT_EXPORT AudioMirroringManager {
 public:
  // Interface for diverting audio data to an alternative AudioOutputStream.
  typedef media::AudioSourceDiverter Diverter;

  // A SourceFrameRef is a RenderFrameHost identified by a <render_process_id,
  // render_frame_id> pair.
  typedef std::pair<int, int> SourceFrameRef;

  // Interface to be implemented by audio mirroring destinations.  See comments
  // for StartMirroring() and StopMirroring() below.
  class MirroringDestination {
   public:
    // Asynchronously query whether this MirroringDestination wants to consume
    // audio sourced from each of the |candidates|.  |results_callback| is run
    // to indicate which of them (or none) should have audio routed to this
    // MirroringDestination.  |results_callback| must be run on the same thread
    // as the one that called QueryForMatches().
    typedef base::Callback<void(const std::set<SourceFrameRef>&)>
        MatchesCallback;
    virtual void QueryForMatches(
        const std::set<SourceFrameRef>& candidates,
        const MatchesCallback& results_callback) = 0;

    // Create a consumer of audio data in the format specified by |params|, and
    // connect it as an input to mirroring.  When Close() is called on the
    // returned AudioOutputStream, the input is disconnected and the object
    // becomes invalid.
    virtual media::AudioOutputStream* AddInput(
        const media::AudioParameters& params) = 0;

   protected:
    virtual ~MirroringDestination() {}
  };

  // Note: Use GetInstance() for non-test code.
  AudioMirroringManager();
  virtual ~AudioMirroringManager();

  // Returns the global instance.
  static AudioMirroringManager* GetInstance();

  // Add/Remove a diverter for an audio stream with a known RenderFrame source
  // (represented by |render_process_id| + |render_frame_id|).  Multiple
  // diverters may be added for the same source frame, but never the same
  // diverter.  |diverter| must live until after RemoveDiverter() is called.
  virtual void AddDiverter(int render_process_id, int render_frame_id,
                           Diverter* diverter);
  virtual void RemoveDiverter(Diverter* diverter);

  // (Re-)Start/Stop mirroring to the given |destination|.  |destination| must
  // live until after StopMirroring() is called.  A client may request a
  // re-start by calling StartMirroring() again; and this will cause
  // AudioMirroringManager to query |destination| and only re-route those
  // diverters that are missing/new to the returned set of matches.
  virtual void StartMirroring(MirroringDestination* destination);
  virtual void StopMirroring(MirroringDestination* destination);

 private:
  friend class AudioMirroringManagerTest;

  struct StreamRoutingState {
    // The source render frame associated with the audio stream.
    SourceFrameRef source_render_frame;

    // The diverter for re-routing the audio stream.
    Diverter* diverter;

    // If not NULL, the audio stream is currently being diverted to this
    // destination.
    MirroringDestination* destination;

    StreamRoutingState(const SourceFrameRef& source_frame,
                       Diverter* stream_diverter);
    ~StreamRoutingState();
  };

  typedef std::vector<StreamRoutingState> StreamRoutes;
  typedef std::vector<MirroringDestination*> Destinations;

  // Helper to find a destination other than |old_destination| for the given
  // |candidates| to be diverted to.
  void InitiateQueriesToFindNewDestination(
      MirroringDestination* old_destination,
      const std::set<SourceFrameRef>& candidates);

  // MirroringDestination query callback.  |matches| contains all RenderFrame
  // sources that will be diverted to |destination|.  If |add_only| is false,
  // then any Diverters currently routed to |destination| but not found in
  // |matches| will be stopped.
  void UpdateRoutesToDestination(MirroringDestination* destination,
                                 bool add_only,
                                 const std::set<SourceFrameRef>& matches);

  // Starts diverting audio to the |new_destination|, if not NULL.  Otherwise,
  // stops diverting audio.
  static void ChangeRoute(StreamRoutingState* route,
                          MirroringDestination* new_destination);

  // Routing table.  Contains one entry for each Diverter.
  StreamRoutes routes_;

  // All active mirroring sessions.
  Destinations sessions_;

  // Used to check that all AudioMirroringManager code runs on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(AudioMirroringManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_AUDIO_MIRRORING_MANAGER_H_
