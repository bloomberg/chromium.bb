// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_MFPLAYER_MFPLAYER_H_
#define MEDIA_TOOLS_MFPLAYER_MFPLAYER_H_

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#ifdef WIN32
#undef WIN32
#endif
#ifdef WINVER
#undef WINVER
#endif

#include <evr.h>  // Subclass IMFAsyncCallback

#include <cassert>

#include "base/basictypes.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "winmm.lib")

struct IMFAsyncResult;
struct IMFMediaEvent;
struct IMFMediaSession;
struct IMFMediaSource;
struct IMFPresentationDescriptor;
struct IMFStreamDescriptor;
struct IMFTopology;
struct IMFTopologyNode;
struct IMFTranscodeProfile;
struct IMFVideoDisplayControl;

namespace mfplayer {

class MFPlayer : public IMFAsyncCallback {
 public:
  enum PlayerState {
    CLOSED = 0,    // No session.
    READY,         // Session was created, ready to open a file.
    OPEN_PENDING,  // Session is opening a file.
    STARTED,       // Session is playing a file.
    PAUSED,        // Session is paused.
    STOPPED,       // Session is stopped (ready to play).
    CLOSING        // Application has closed the session, but is waiting for
                   // MESessionClosed.
  };

  // This is used to represent a type of event posted to the event_window_.
  static const UINT WM_APP_PLAYER_EVENT = WM_APP + 1;

  // Use this to construct a MFPlayer instead of constructor.
  static bool CreateInstance(HWND video_window, HWND event_window,
                             bool render_to_window, MFPlayer** player);

  // Override IUnknown. STDMETHODCALLTYPE, which expands to __stdcall, is
  // necessary for overriding the parent class's method.
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object_ptr);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  // Implement IMFAsyncCallback.
  HRESULT STDMETHODCALLTYPE GetParameters(DWORD* flags, DWORD* queue) {
    // Implementation of this method is optional, assume default behavior.
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult* result);

  // Opens the file specified in url and sends a message to start playback,
  // once topology has been completed. For archive sinks, out_url is the
  // destination at which the encoded file will be saved.
  bool OpenURL(const WCHAR* in_url, const WCHAR* out_url);

  // Starts playback.
  bool Play();

  // Pauses playback.
  bool Pause();

  // Shuts down the media player. Called by the destructor.
  bool Shutdown();

  // Handles messages in the message queue.
  HRESULT HandleEvent(UINT_PTR event_ptr);
  inline PlayerState state() const { return state_; }

  // Video functionality
  bool Repaint();
  bool ResizeVideo(WORD width, WORD height);
  inline bool HasVideo() const { return (video_display_ != NULL); }

  // The following methods are used for archive sink.
  // Begins the encoding operation. Call this after OpenURL() only.
  bool Transcode();

 private:
  // Private constructor: Call CreateInstance() instead.
  MFPlayer(HWND video_window, HWND event_window,
                     bool render_to_window);

  // Caller should use Release() instead, which keeps track of ref count.
  ~MFPlayer();

  // Initializes the media player. This actually only creates an Event object
  // to signal closing the media player.
  HRESULT Initialize();

  // Creates the media session object of the player, and starts pulling events
  // from it.
  HRESULT CreateSession();

  // The meat of Shutdown(). Releases the source and the session.
  HRESULT CloseSession();
  HRESULT StartPlayback();
  HRESULT CreateMediaSource(const WCHAR* url);

  // Creates the partial topology of the player.
  HRESULT CreateTopologyFromSource(IMFTopology** topology);

  // Called by CreateTopologyFromSource() to add nodes to the topology, if the
  // stream is either video or audio stream.
  HRESULT AddBranchToPartialTopology(
      IMFTopology* topology,
      IMFPresentationDescriptor* presentation_desc,
      DWORD index);
  HRESULT CreateSourceStreamNode(IMFPresentationDescriptor* presentation_desc,
                                 IMFStreamDescriptor* stream_desc,
                                 IMFTopologyNode** node_ptr);
  HRESULT CreateOutputNode(IMFStreamDescriptor* stream_desc,
                           IMFTopologyNode** node_ptr);

  // Media event handlers
  HRESULT OnTopologyReady(IMFMediaEvent* event);
  HRESULT OnPresentationEnded(IMFMediaEvent* event);

  // These methods are used for archive sink.
  HRESULT ConfigureTranscodeAudioOutput();
  HRESULT ConfigureTranscodeVideoOutput();
  HRESULT ConfigureContainer();

  LONG ref_count_;
  PlayerState state_;
  IMFMediaSession* session_;
  IMFMediaSource* source_;
  IMFVideoDisplayControl* video_display_;
  HWND video_window_;
  HWND event_window_;
  HANDLE close_event_;
  bool render_to_window_;
  IMFTranscodeProfile* transcode_profile_;

  DISALLOW_COPY_AND_ASSIGN(MFPlayer);
};

}  // namespace mfplayer

#endif  // MEDIA_TOOLS_MFPLAYER_MFPLAYER_H_
