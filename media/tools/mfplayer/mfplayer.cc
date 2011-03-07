// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/tools/mfplayer/mfplayer.h"

#include <mfapi.h>
#include <mferror.h>
#include <shlwapi.h>
#include <strsafe.h>

#include <cassert>

template <class T>
static void SafeRelease(T** pptr) {
  if (pptr && *pptr) {
    (*pptr)->Release();
    *pptr = NULL;
  }
}

static HRESULT ProbeTopology(IMFMediaEvent* event,
                             IMFTopology** topology_ptr) {
  HRESULT hr = S_OK;
  PROPVARIANT var;
  PropVariantInit(&var);
  hr = event->GetValue(&var);
  if (SUCCEEDED(hr)) {
    if (var.vt != VT_UNKNOWN)
      hr = E_UNEXPECTED;
  }
  if (SUCCEEDED(hr))
    hr = var.punkVal->QueryInterface(IID_PPV_ARGS(topology_ptr));
  PropVariantClear(&var);
  return hr;
}

namespace mfplayer {

// Public methods

bool MFPlayer::CreateInstance(HWND video_window, HWND event_window,
                                 bool render_to_window, MFPlayer** player) {
  if (!player)
    return false;

  HRESULT hr = S_OK;
  MFPlayer* temp_player = new MFPlayer(video_window, event_window,
                                       render_to_window);
  if (!temp_player)
    return false;

  hr = temp_player->Initialize();
  if (SUCCEEDED(hr)) {
    *player = temp_player;
    (*player)->AddRef();
  }
  // If above succeeded, then ref count is now 2, so SafeRelease won't delete
  // the object.
  SafeRelease(&temp_player);
  return SUCCEEDED(hr);
}

// Override IUnknown
HRESULT MFPlayer::QueryInterface(REFIID id, void** object_ptr) {
  static const QITAB qit[] = { QITABENT(MFPlayer, IMFAsyncCallback), {0}};
  return QISearch(this, qit, id, object_ptr);
}

ULONG MFPlayer::AddRef() {
  return InterlockedIncrement(&ref_count_);
}

ULONG MFPlayer::Release() {
  ULONG count = InterlockedDecrement(&ref_count_);
  if (count == 0)
    delete this;
  return count;
}

// IMFAsyncCallback implementation
// Callback for asynchronous BeginGetEvent method.
HRESULT MFPlayer::Invoke(IMFAsyncResult* result) {
  IMFMediaEvent* event = NULL;
  MediaEventType event_type = MEUnknown;

  HRESULT hr = session_->EndGetEvent(result, &event);
  if (FAILED(hr))
    goto done;
  hr = event->GetType(&event_type);
  if (FAILED(hr))
    goto done;

  // |session_|->Close() queues an event of this type.
  // If the session is closed, the application is waiting on the event
  // handle. Also, do not request any more events from the session.
  if (event_type == MESessionClosed) {
    SetEvent(close_event_);
  } else {
    // For all other events, ask the media session for the
    // next event in the queue.
    hr = session_->BeginGetEvent(this, NULL);
    if (FAILED(hr))
      goto done;
  }
  // For most events, post the event as a private window message to the
  // application. This lets the application process the event on its main
  // thread.

  // However, if a call to IMFMediaSession::Close is pending, it means the
  // application is waiting on the m_hCloseEvent event handle. (Blocking
  // call.) In that case, we simply discard the event.

  // When IMFMediaSession::Close is called, MESessionClosed is NOT
  // necessarily the next event that we will receive. We may receive any
  // number of other events before receiving MESessionClosed.
  if (state_ != CLOSING) {
    event->AddRef();
    // Post this event onto the event_window_'s queue. When an event is
    // received, the app calls MFPlayer::HandleEvent() to handle the event.
    PostMessage(event_window_, WM_APP_PLAYER_EVENT, (WPARAM)event, NULL);
  }
 done:
  return S_OK;
}

// This method does the following:
// 1. Create a new media session.
// 2. Create the media source.
// 3. Create the topology.
// 4. Queue the topology [asynchronous]
// 5. Start playback [asynchronous - does not happen in this method.]
bool MFPlayer::OpenURL(const WCHAR* in_url, const WCHAR* out_url) {
  IMFTopology* topology = NULL;
  HRESULT hr;

  // Initializes |session_|.
  hr = CreateSession();
  if (FAILED(hr))
    goto done;
  // Initializes |source_|.
  hr = CreateMediaSource(in_url);
  if (FAILED(hr))
    goto done;
  if (render_to_window_) {
    // Create a partial topology.
    hr = CreateTopologyFromSource(&topology);
    if (FAILED(hr))
      goto done;
  } else {
    // Creating topology for a transcoder requires a somewhat different setup.
    fprintf(stderr, "OpenURL: using archive sink\n");
    hr = MFCreateTranscodeProfile(&transcode_profile_);
    if (FAILED(hr))
      goto done;
    /*
    hr = ConfigureTranscodeAudioOutput();
    if (FAILED(hr))
      goto done;
    fprintf(stderr, "OpenURL: Configured audio output\n");
    */
    hr = ConfigureTranscodeVideoOutput();
    if (FAILED(hr))
      goto done;
    fprintf(stderr, "OpenURL: Configured video output\n");
    hr = ConfigureContainer();
    if (FAILED(hr))
      goto done;
    fprintf(stderr, "OpenURL: Configured container\n");
    hr = MFCreateTranscodeTopology(source_, out_url, transcode_profile_,
                                   &topology);
    if (FAILED(hr))
      goto done;
    fprintf(stderr, "OpenURL: Created transcode topology\n");
  }
  // First argument is flags (which is none).
  hr = session_->SetTopology(0, topology);
  if (FAILED(hr))
    goto done;
  fprintf(stderr, "Added topology to session\n");
  // SetTopology() is an asynchronous method. If it succeeds, the media
  // session will queue an MESessionTopologySet event.
  // SetTopology() without MFSESSION_SETTOPOLOGY_NORESOLUTION means the
  // topology is to be resolved.
  state_ = OPEN_PENDING;
 done:
  if (FAILED(hr))
    state_ = CLOSED;
  SafeRelease(&topology);
  return SUCCEEDED(hr);
}

bool MFPlayer::Play() {
  if (state_ != PAUSED && state_ != STOPPED)
    return false;
  if (!session_ || !source_)
    return false;
  HRESULT hr = StartPlayback();
  return SUCCEEDED(hr);
}

bool MFPlayer::Pause() {
  if (state_ != STARTED)
    return false;
  if (!session_ || !source_)
    return false;
  HRESULT hr = session_->Pause();
  if (SUCCEEDED(hr))
    state_ = PAUSED;
  return SUCCEEDED(hr);
}

bool MFPlayer::Shutdown() {
  HRESULT hr = CloseSession();
  if (close_event_) {
    CloseHandle(close_event_);
    close_event_ = NULL;
  }
  return SUCCEEDED(hr);
}

// Callback from application upon receiving an WM_APP_PLAYER event.
// This method is used to process media session events on the
// application's main thread.
HRESULT MFPlayer::HandleEvent(UINT_PTR event_ptr) {
  IUnknown* unknown_ptr = reinterpret_cast<IUnknown*>(event_ptr);
  IMFMediaEvent* event = NULL;

  if (!unknown_ptr)
    return E_POINTER;
  // Incremented ref count.
  HRESULT hr = unknown_ptr->QueryInterface(IID_PPV_ARGS(&event));
  if (FAILED(hr))
    goto done;
  MediaEventType event_type;
  HRESULT event_status = S_OK;
  hr = event->GetType(&event_type);
  if (FAILED(hr))
    goto done;
  hr = event->GetStatus(&event_status);
  if (FAILED(hr))
    goto done;
  // Check if the async operation succeeded.
  if (FAILED(event_status)) {
    hr = event_status;
    goto done;
  }

  // Handle the event according to its type.
  MF_TOPOSTATUS topology_status = MF_TOPOSTATUS_INVALID;
  switch (event_type) {
    case MESessionTopologySet:
      hr = OnTopologyReady(event);
      break;
    case MEEndOfPresentation:
      hr = OnPresentationEnded(event);
      break;
      // The MENewPresentation event signals the start of a new presentation.
      // However, in many cases, you will not receive this event at all.
      // It seems like there is no need to handle other events, but if there
      // is a need in the future they can be added here.
  }

 done:
  SafeRelease(&event);
  SafeRelease(&unknown_ptr);
  return hr;
}

// Video functionality implementation
// Called by OnPaint() in application, when it receives a WM_PAINT message.
// The EVR (video_display_) must be notified, i.e. repaint.
bool MFPlayer::Repaint() {
  if (video_display_)
    return SUCCEEDED(video_display_->RepaintVideo());
  else
    return true;
}

// Called when application receives a WM_SIZE message.
bool MFPlayer::ResizeVideo(WORD Width, WORD Height) {
  if (video_display_) {
    // Fields are: Left (x), Top (y), Right (x), Bottom (y)
    RECT DestRect = { 0, 0, Width, Height };
    // First parameter is the "source rectangle" - NULL means show
    // the entire portion of the original rectangle. Default is
    // {0, 0, 1, 1}.
    return SUCCEEDED(video_display_->SetVideoPosition(NULL, &DestRect));
  } else {
    return true;
  }
}

// Private methods

MFPlayer::MFPlayer(HWND video_window, HWND event_window,
                   bool render_to_window)
    : ref_count_(1),
      state_(CLOSED),
      session_(NULL),
      source_(NULL),
      video_display_(NULL),
      video_window_(video_window),
      event_window_(event_window),
      close_event_(NULL),
      render_to_window_(render_to_window),
      transcode_profile_(NULL) {
}

MFPlayer::~MFPlayer() {
  // If false, app did not call Shutdown().
  assert(!session_);

  // When MFPlayer calls IMediaEventGenerator::BeginGetEvent on the
  // media session, it causes the media session to hold a reference
  // count on the MFPlayer.
  // This creates a circular reference count between MFPlayer and the
  // media session. Calling Shutdown breaks the circular reference
  // count.
  // If CreateInstance fails, the application will not call
  // Shutdown. To handle that case, call Shutdown in the destructor.
  Shutdown();
}

// This method only creates the close event. This method is called from
// CreateInstance().
HRESULT MFPlayer::Initialize() {
  if (close_event_)
    return MF_E_ALREADY_INITIALIZED;
  HRESULT hr = S_OK;
  close_event_ = CreateEvent(NULL,   // Not inheritable by child process, if any
                             FALSE,  // Auto-reset
                             FALSE,  // Not signaled initially
                             NULL);  // No name
  if (!close_event_)
    hr = HRESULT_FROM_WIN32(GetLastError());
  return hr;
}

HRESULT MFPlayer::CreateSession() {
  // Close previous section, if any.
  HRESULT hr = CloseSession();
  if (FAILED(hr))
    goto done;

  assert(state_ == CLOSED);

  // First argument is optional configurations. (Which is none)
  hr = MFCreateMediaSession(NULL, &session_);
  if (FAILED(hr))
    goto done;

  state_ = READY;
  if (render_to_window_) {
    // Start pulling events from the media session. It calls back the object's
    // Invoke method when there is a new event available.
    hr = session_->BeginGetEvent(this, NULL);
    if (FAILED(hr))
      goto done;
  }
 done:
  return hr;
}

// Closes the media session. Called from either Shutdown() or CreateSession().
// The IMFMediaSession::Close method is asynchronous, but the CloseSession
// method waits on the MESessionClosed event. The MESessionClosed event is
// guaranteed to be the last event that the media session fires.
HRESULT MFPlayer::CloseSession() {
  HRESULT hr = S_OK;

  SafeRelease(&video_display_);
  if (session_) {
    state_ = CLOSING;
    hr = session_->Close();
    if (FAILED(hr))
      goto done;
    // Wait 5 seconds for the close operation to complete, i.e. the close event
    // to be signaled. close_event_ only exists for playback mode.
    if (render_to_window_)
      WaitForSingleObject(close_event_, 5000);
    // Now there will (or should) be no more events from this session.
  }
  // Complete shutdown operations.
  // Note: Shutdown()s are synchronous.
  if (source_)
    source_->Shutdown();
  if (session_)
    session_->Shutdown();
  SafeRelease(&source_);
  SafeRelease(&session_);
  SafeRelease(&transcode_profile_);
  state_ = CLOSED;

 done:
  return hr;
}

// Starts the session. In the context of playback, it means starting the
// playback. In the context of writing to file, it means starting the encoding
// operation.
HRESULT MFPlayer::StartPlayback() {
  assert(session_);
  PROPVARIANT var_start;
  PropVariantInit(&var_start);
  var_start.vt = VT_EMPTY;
  HRESULT hr = session_->Start(&GUID_NULL, &var_start);
  if (SUCCEEDED(hr)) {
    // Start is an asynchronous operation. However, we can treat our state
    // as being already started.
    state_ = STARTED;
  }
  PropVariantClear(&var_start);
  return hr;
}

HRESULT MFPlayer::CreateMediaSource(const WCHAR* url) {
  MF_OBJECT_TYPE object_type = MF_OBJECT_INVALID;
  IMFSourceResolver* resolver = NULL;
  IUnknown* source = NULL;

  // Release the old source.
  SafeRelease(&source_);

  HRESULT hr = MFCreateSourceResolver(&resolver);
  if (FAILED(hr))
    goto done;

  // Use resolver to create media source.
  // Synchronous method, use BeginCreateObjectFromUrl for asynchronous.
  hr = resolver->CreateObjectFromURL(url,
                                     MF_RESOLUTION_MEDIASOURCE,
                                     NULL,         // Not passing in properties.
                                     &object_type,
                                     &source);     // Receives created source.
  if (FAILED(hr))
    goto done;

  // Get the IMFMediaSource interface from the media source, i.e. initializes
  // source_.
  // This call increments the ref count of returned interface.
  hr = source->QueryInterface(IID_PPV_ARGS(&source_));

 done:
  // Free up resources, if any were acquired.
  SafeRelease(&resolver);
  SafeRelease(&source);
  return hr;
}

HRESULT MFPlayer::CreateTopologyFromSource(IMFTopology** topology) {
  assert(session_);
  assert(source_);

  IMFTopology* temp_topology = NULL;
  IMFPresentationDescriptor* presentation_desc = NULL;
  DWORD num_streams = 0;

  HRESULT hr = MFCreateTopology(&temp_topology);
  if (FAILED(hr))
    goto done;
  hr = source_->CreatePresentationDescriptor(&presentation_desc);
  if (FAILED(hr))
    goto done;
  hr = presentation_desc->GetStreamDescriptorCount(&num_streams);
  if (FAILED(hr))
    goto done;
  // For each stream, create the topology nodes and add them to the topology.
  for (DWORD i = 0; i < num_streams; i++) {
    hr = AddBranchToPartialTopology(temp_topology, presentation_desc, i);
    if (FAILED(hr))
      goto done;
  }
  *topology = temp_topology;
  (*topology)->AddRef();

 done:
  SafeRelease(&temp_topology);
  SafeRelease(&presentation_desc);
  return hr;
}

// Creates a topology node with stream of given index and adds the node to the
// topology.
// Add a topology branch for one stream.
// For each stream, this function does the following:
//   1. Creates a source node associated with the stream.
//   2. Creates an output node for the renderer.
//   3. Connects the two nodes.
// The media session will add any decoders that are needed.
HRESULT MFPlayer::AddBranchToPartialTopology(
    IMFTopology* topology, IMFPresentationDescriptor* presentation_desc,
    DWORD index) {
  assert(topology);

  IMFStreamDescriptor* stream_desc = NULL;
  IMFMediaTypeHandler* handler = NULL;
  GUID major_type_id = GUID_NULL;
  IMFTopologyNode* source_node = NULL;
  IMFTopologyNode* output_node = NULL;
  BOOL stream_selected = FALSE;

  HRESULT hr = presentation_desc->GetStreamDescriptorByIndex(index,
                                                             &stream_selected,
                                                             &stream_desc);
  if (FAILED(hr))
    goto done;
  // Create topology branch only if stream is selected.
  if (stream_selected) {
    hr = stream_desc->GetMediaTypeHandler(&handler);
    if (FAILED(hr))
      goto done;
    hr = handler->GetMajorType(&major_type_id);
    if (FAILED(hr))
      goto done;
    if (major_type_id != MFMediaType_Video)
      goto done;
    hr = CreateSourceStreamNode(presentation_desc, stream_desc, &source_node);
    if (FAILED(hr))
      goto done;
    hr = CreateOutputNode(stream_desc, &output_node);
    if (FAILED(hr))
      goto done;
    // Add both nodes to the topology.
    hr = topology->AddNode(source_node);
    if (FAILED(hr))
      goto done;
    hr = topology->AddNode(output_node);
    if (FAILED(hr))
      goto done;
    // Connect the 0-th output stream of source node to the 0-th input stream of
    // the output node.
    hr = source_node->ConnectOutput(0, output_node, 0);
  }

 done:
  SafeRelease(&stream_desc);
  SafeRelease(&source_node);
  SafeRelease(&output_node);
  SafeRelease(&handler);
  return hr;
}

HRESULT MFPlayer::CreateSourceStreamNode(
    IMFPresentationDescriptor* presentation_desc,
    IMFStreamDescriptor* stream_desc, IMFTopologyNode** node_ptr) {
  if (!source_ || !presentation_desc || !stream_desc || !node_ptr)
    return E_POINTER;
  IMFTopologyNode* temp_node = NULL;
  HRESULT hr = MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &temp_node);
  if (FAILED(hr))
    goto done;
  // Set attribute of top node source to source_.
  hr = temp_node->SetUnknown(MF_TOPONODE_SOURCE, source_);
  if (FAILED(hr))
    goto done;
  // Set attribute of presentation descriptor.
  hr = temp_node->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR,
                             presentation_desc);
  if (FAILED(hr))
    goto done;
  // Set attribute of stream descriptor.
  hr = temp_node->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, stream_desc);
  if (FAILED(hr))
    goto done;
  *node_ptr = temp_node;
  (*node_ptr)->AddRef();

 done:
  SafeRelease(&temp_node);
  return hr;
}

// This function does the following:
// 1. Chooses a renderer based on the media type of the stream.
// 2. Creates an IActivate object for the renderer. IActivate objects allow the
//    application to defer its creation.
// 3. Creates an output topology node.
// 4. Sets the IActivate pointer on the node.
HRESULT MFPlayer::CreateOutputNode(IMFStreamDescriptor* stream_desc,
                                   IMFTopologyNode** node_ptr) {
  IMFTopologyNode* temp_node = NULL;
  IMFMediaTypeHandler* handler = NULL;
  IMFActivate* renderer_activate = NULL;
  GUID major_type_id = GUID_NULL;

  HRESULT hr = stream_desc->GetMediaTypeHandler(&handler);
  if (FAILED(hr))
    goto done;

  // Get the type of the stream (video, audio, etc.)
  hr = handler->GetMajorType(&major_type_id);
  if (FAILED(hr))
    goto done;

  // Is it a video stream, audio stream, etc.?
  // Create an IMFActivate object for the renderer, based on the media type
  if (major_type_id == MFMediaType_Video) {
    assert(video_window_);
    hr = MFCreateVideoRendererActivate(video_window_, &renderer_activate);
  } else {
    hr = E_FAIL;
  }
  if (FAILED(hr))
    goto done;

  hr = MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &temp_node);
  if (FAILED(hr))
    goto done;

  // Associate the activate object with the node.
  hr = temp_node->SetObject(renderer_activate);
  if (FAILED(hr))
    goto done;

  *node_ptr = temp_node;
  (*node_ptr)->AddRef();

 done:
  SafeRelease(&temp_node);
  SafeRelease(&handler);
  SafeRelease(&renderer_activate);
  return hr;
}

// Handler for MESessionTopologySet.
// This situation means the new topology is ready for playback. After this
// event is received, any calls to IMFGetService will get service interfaces
// from the new topology.
HRESULT MFPlayer::OnTopologyReady(IMFMediaEvent* event) {
//////////
  IMFTopology* full_topology = NULL;
  HRESULT hr;
  hr = ProbeTopology(event, &full_topology);
  if (FAILED(hr))
    fprintf(stderr, "Failed to probe topology\n");
  fprintf(stderr, "Probed topology\n");

  WORD num_nodes_in_topology;
  hr = full_topology->GetNodeCount(&num_nodes_in_topology);
  if (FAILED(hr)) {
    fprintf(stderr, "GetNodeCount failed\n");
    goto release_topology;
  }
  fprintf(stderr, "There are %d nodes in topology\n", num_nodes_in_topology);

  for (WORD i = 0; i < num_nodes_in_topology; i++) {
    IMFTopologyNode* node = NULL;
    hr = full_topology->GetNode(i, &node);
    if (FAILED(hr)) {
      fprintf(stderr, "failed to get node %d\n", i);
    } else {
      fprintf(stderr, "got node %d\n", i);
    }
    SafeRelease(&node);
  }

 release_topology:
  SafeRelease(&full_topology);
  if (render_to_window_) {
    // Release the old IMFVideoDisplayControl.
    SafeRelease(&video_display_);
    // Ask for the IMFVideoDisplayControl interface. This interface is
    // implemented by the EVR and is exposed by the media session as a service.
    // Note: This call is expected to fail if the source does not have video.
    MFGetService(session_, MR_VIDEO_RENDER_SERVICE,
                 IID_PPV_ARGS(&video_display_));
    // Note: Audio, Video acceleration are also services that can be obtained.
  }
  hr = StartPlayback();
  return hr;
}

// Handler for MEEndOfPresentation event.
HRESULT MFPlayer::OnPresentationEnded(IMFMediaEvent* event) {
  state_ = STOPPED;
  return S_OK;
}

HRESULT MFPlayer::ConfigureTranscodeAudioOutput() {
  assert(transcode_profile_);

  HRESULT hr = S_OK;
  DWORD num_available_formats;
  IMFCollection* available_types = NULL;
  IUnknown* unknown_audio_type = NULL;
  IMFMediaType* audio_type = NULL;
  IMFAttributes* audio_attributes = NULL;
  const GUID OUTPUT_AUDIO_SUBTYPE = MFAudioFormat_AAC;
  // Get all available types of output formats.
  hr = MFTranscodeGetAudioOutputAvailableTypes(OUTPUT_AUDIO_SUBTYPE,
                                               MFT_ENUM_FLAG_ALL,
                                               NULL,
                                               &available_types);
  if (SUCCEEDED(hr)) {
    hr = available_types->GetElementCount(&num_available_formats);
    if (num_available_formats == 0)
      hr = E_UNEXPECTED;
  }
  if (SUCCEEDED(hr)) {
    // Just pick the first available format.
    hr = available_types->GetElement(0, &unknown_audio_type);
  }
  if (SUCCEEDED(hr))
    hr = unknown_audio_type->QueryInterface(IID_PPV_ARGS(&audio_type));
  if (SUCCEEDED(hr))
    hr = MFCreateAttributes(&audio_attributes, 0);
  if (SUCCEEDED(hr))
    hr = audio_type->CopyAllItems(audio_attributes);
  if (SUCCEEDED(hr))
    hr = audio_attributes->SetGUID(MF_MT_SUBTYPE, OUTPUT_AUDIO_SUBTYPE);

  if (SUCCEEDED(hr))
    hr = transcode_profile_->SetAudioAttributes(audio_attributes);

  SafeRelease(&available_types);
  SafeRelease(&unknown_audio_type);
  SafeRelease(&audio_type);
  SafeRelease(&audio_attributes);
  return hr;
}

HRESULT MFPlayer::ConfigureTranscodeVideoOutput() {
  assert(transcode_profile_);

  HRESULT hr = S_OK;
  IMFAttributes* video_attributes = NULL;
  // We set 5 attributes for our video output:
  // 1. Output subtype
  // 2. Frame rate
  // 3. Frame Size
  // 4. Aspect ratio
  // 5. Bit rate
  hr = MFCreateAttributes(&video_attributes, 5);
  if (SUCCEEDED(hr))
    hr = video_attributes->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
  if (SUCCEEDED(hr))
    hr = MFSetAttributeRatio(video_attributes, MF_MT_FRAME_RATE, 30, 1);
  if (SUCCEEDED(hr))
    hr = MFSetAttributeSize(video_attributes, MF_MT_FRAME_SIZE, 320, 240);
  if (SUCCEEDED(hr))
    hr = MFSetAttributeRatio(video_attributes, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
  if (SUCCEEDED(hr))
    hr = video_attributes->SetUINT32(MF_MT_AVG_BITRATE, 300000);

  if (SUCCEEDED(hr))
    hr = transcode_profile_->SetVideoAttributes(video_attributes);

  SafeRelease(&video_attributes);
  return hr;
}

HRESULT MFPlayer::ConfigureContainer() {
  assert(transcode_profile_);

  HRESULT hr = S_OK;
  IMFAttributes* container_attributes = NULL;

  hr = MFCreateAttributes(&container_attributes, 2);
  // Make the container of MP4 type.
  if (SUCCEEDED(hr))
    hr = container_attributes->SetGUID(MF_TRANSCODE_CONTAINERTYPE,
                                      MFTranscodeContainerType_MPEG4);
  // Use the default setting. Media Foundation will use the stream
  // settings set in ConfigureAudioOutput and ConfigureVideoOutput.
  if (SUCCEEDED(hr))
    hr = container_attributes->SetUINT32(MF_TRANSCODE_ADJUST_PROFILE,
                                        MF_TRANSCODE_ADJUST_PROFILE_DEFAULT);

  if (SUCCEEDED(hr))
    hr = transcode_profile_->SetContainerAttributes(container_attributes);

  SafeRelease(&container_attributes);
  return hr;
}

// This method will start getting media session events synchronously.
bool MFPlayer::Transcode() {
  assert(session_);

  IMFMediaEvent* event = NULL;
  MediaEventType event_type = MEUnknown;
  HRESULT hr = S_OK;
  HRESULT event_status = S_OK;

  while (event_type != MESessionClosed) {
    hr = session_->GetEvent(0, &event);
    if (FAILED(hr))
      break;
    hr = event->GetType(&event_type);
    if (FAILED(hr))
      break;
    hr = event->GetStatus(&event_status);
    if (FAILED(hr))
      break;
    if (FAILED(event_status)) {
      hr = event_status;
      break;
    }
    switch (event_type) {
      case MESessionTopologySet:
        hr = OnTopologyReady(event);
        break;
      case MESessionStarted:
        break;
      case MESessionEnded:
        hr = session_->Close();
        // Finished encoding.
        break;
      case MESessionClosed:
        // Output file has been created - exit on next iteration.
        break;
    }
    if (FAILED(hr))
      break;
    SafeRelease(&event);
  }
  // Release the last MESessionClosed event.
  SafeRelease(&event);
  return SUCCEEDED(hr);
}

}  // namespace mfplayer
