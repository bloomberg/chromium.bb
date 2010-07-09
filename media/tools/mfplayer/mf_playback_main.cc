// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header is placed first to avoid compiler warning.
#include "media/tools/mfplayer/mfplayer.h"

#include <mfapi.h>
#include <strsafe.h>
#include <cassert>

namespace mfplayer {

const wchar_t g_window_title[] = L"MFBasicPlayback";
const wchar_t g_window_class[] = L"Chrome_MFBasicPlayback";

// True if there is no video playing, so we have to paint the window ourselves.
bool g_repaint_client = true;

// Global player object.
// Note: After WM_CREATE is processed, g_main_player remains valid until the
// window is destroyed.
MFPlayer* g_main_player = NULL;

wchar_t* g_input_file_name = NULL;

void Usage() {
  fprintf(stderr, "Usage: mf_playback (-r|-f) (-h|-s) file [out-file]\n");
}

void UpdateUI(HWND window_handle, MFPlayer::PlayerState state) {
  assert(g_main_player != NULL);

  bool playback_started = state == MFPlayer::STARTED ||
                          state == MFPlayer::PAUSED;
  g_repaint_client = !playback_started || !g_main_player->HasVideo();
}

//  Shows a message box with an error message.
void NotifyError(HWND window_handle, PCWSTR error_message, HRESULT error_code) {
  const size_t kMessageLen = 512;
  wchar_t message[kMessageLen];

  HRESULT hr = StringCchPrintf(message, kMessageLen, L"%s (HRESULT = 0x%X)",
                               error_message, error_code);
  if (SUCCEEDED(hr))
    MessageBox(window_handle, message, NULL, MB_OK | MB_ICONERROR);
}

// Converts a ANSI string to an Unicode string. It is caller's responsibility
// to call delete[] on the returned string when it is no longer being used.
int ConvertANSIStringToUnicode(const char* source, wchar_t** dest) {
  DWORD string_length = MultiByteToWideChar(CP_ACP, 0, source, -1, NULL, 0);
  if (string_length == 0) {
    fprintf(stderr, "Error getting size of ansi string: %s\n", source);
    return 1;
  }
  *dest = new wchar_t[string_length];
  if (*dest == NULL) {
    fprintf(stderr, "Error allocating unicode string buffer\n");
    return 1;
  }
  if (MultiByteToWideChar(CP_ACP, 0, source, string_length, *dest,
                          string_length) == 0) {
    fprintf(stderr, "Error converting ansi string to unicode: %#X",
            GetLastError());
    return 1;
  }
  return 0;
}

// Callback when window is created. It has been modified to both create
// the media player and starts playing the provided file in one go.
LRESULT OnCreateWindow(HWND window_handle) {
  // Initialize the player object.
  HRESULT hr = MFPlayer::CreateInstance(window_handle, window_handle,
                                        true, &g_main_player);
  if (SUCCEEDED(hr)) {
    UpdateUI(window_handle, MFPlayer::CLOSED);
  } else {
    NotifyError(NULL, L"Could not initialize the player object.", hr);
    return -1;  // Destroy the window
  }

  // Play the video
  assert(g_input_file_name != NULL);

  // No output URL, just the window.
  hr = g_main_player->OpenURL(g_input_file_name, NULL);
  if (SUCCEEDED(hr)) {
    UpdateUI(window_handle, MFPlayer::OPEN_PENDING);
    return 0;
  } else {
    _fwprintf_p(stderr, L"Fatal error: cannot open file %s\n",
                g_input_file_name);
    return -1;  // Destroy the window
  }
}

void OnPaint(HWND window_handle) {
  if (g_repaint_client) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(window_handle, &ps);
    // The video is not playing, so we must paint the application window.
    RECT rc;
    GetClientRect(window_handle, &rc);
    FillRect(hdc, &rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW));
    EndPaint(window_handle, &ps);
  } else {
    // Video is playing. Ask the player to repaint.
    g_main_player->Repaint();
  }
}

void OnKeyPress(WPARAM key) {
  switch (key) {
    // Space key toggles between running and paused.
    case VK_SPACE:
      if (g_main_player->state() == MFPlayer::STARTED)
        g_main_player->Pause();
      else if (g_main_player->state() == MFPlayer::PAUSED)
        g_main_player->Play();
      break;
  }
}

void OnPlayerEvent(HWND window_handle, WPARAM event) {
  HRESULT hr = S_OK;

  hr = g_main_player->HandleEvent(event);
  if (FAILED(hr))
    NotifyError(window_handle, L"An error occurred.", hr);
  UpdateUI(window_handle, g_main_player->state());
}

LRESULT CALLBACK CallbackProc(HWND window_handle, UINT message, WPARAM w_param,
                         LPARAM l_param) {
  switch (message) {
    case WM_CREATE:
      return OnCreateWindow(window_handle);
    case WM_PAINT:
      OnPaint(window_handle);
      break;
    case WM_SIZE:
      g_main_player->ResizeVideo(LOWORD(l_param), HIWORD(l_param));
      break;
    case WM_ERASEBKGND:
      // Suppress window erasing, to reduce flickering while the video is
      // playing.
      return 1;
    case WM_DESTROY:
      PostQuitMessage(0);
      break;
    case WM_CHAR:
      OnKeyPress(w_param);
      break;
    case MFPlayer::WM_APP_PLAYER_EVENT:
      OnPlayerEvent(window_handle, w_param);
      break;
    default:
      return DefWindowProc(window_handle, message, w_param, l_param);
  }
  return 0;
}

// Creates a window for doing playback.
bool InitInstance() {
  HWND window_handle;
  WNDCLASSEX window_class_info;

  // Register the window class.
  ZeroMemory(&window_class_info, sizeof(WNDCLASSEX));
  window_class_info.cbSize = sizeof(WNDCLASSEX);
  window_class_info.style = CS_HREDRAW | CS_VREDRAW;
  window_class_info.lpfnWndProc = CallbackProc;
  window_class_info.hInstance = NULL;
  window_class_info.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
  window_class_info.lpszMenuName = NULL;
  window_class_info.lpszClassName = g_window_class;

  if (RegisterClassEx(&window_class_info) == 0)
    return false;

  // Create the application window.
  window_handle = CreateWindow(g_window_class, g_window_title,
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
                               CW_USEDEFAULT, 0, NULL, NULL, NULL, NULL);
  if (window_handle == 0) {
    fprintf(stderr, "Cannot create window\n");
    return false;
  }

  ShowWindow(window_handle, SW_SHOW);
  UpdateWindow(window_handle);
  return true;
}

int RunRendererApp() {
  if (!InitInstance()) {
    fprintf(stderr, "Could not initialize the application\n");
    return 1;
  }
  MSG message;
  ZeroMemory(&message, sizeof(message));
  // Main message loop.
  while (GetMessage(&message, NULL, 0, 0)) {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }
  return 0;
}

int RunTranscoderApp(const char* output_file_name) {
  HRESULT hr;
  int ret = 0;

  // In here, we do not use any windows - just use the media player as a
  // transcoder.
  hr = MFPlayer::CreateInstance(NULL, NULL, false, &g_main_player);
  if (FAILED(hr)) {
    fprintf(stderr, "Transcoder app: Could not create player\n");
    return 1;
  }
  wchar_t* output_file_name_wchar = NULL;
  if (ConvertANSIStringToUnicode(output_file_name,
                                 &output_file_name_wchar) != 0) {
    fprintf(stderr, "Fatal error while converting output_file_name\n");
    return 1;
  }
  hr = g_main_player->OpenURL(g_input_file_name, output_file_name_wchar);
  if (FAILED(hr)) {
    fprintf(stderr, "Failed to open URL and configure transcoder profile\n");
    ret = 1;
    goto cleanup;
  }
  fprintf(stderr, "Transcode starting now, writing to %s\n", output_file_name);
  hr = g_main_player->Transcode();
  if (FAILED(hr)) {
    fprintf(stderr, "Transcode failed\n");
    ret = 1;
    goto cleanup;
  }
  if (SUCCEEDED(hr))
    fprintf(stderr, "Transcode successful\n");
 cleanup:
  delete[] output_file_name_wchar;
  return ret;
}

int main2(int argc, char** argv) {
  bool use_renderer_sink;
  bool enable_dxva2;
  if (argc < 4) {
    Usage();
    return 1;
  }
  if (strcmp(argv[1], "-r") == 0) {
    use_renderer_sink = true;
  } else if (strcmp(argv[1], "-f") == 0) {
    use_renderer_sink = false;
    if (argc < 5) {
      fprintf(stderr, "Archive sink specified but out-file missing\n");
      Usage();
      return 1;
    }
  } else {
    fprintf(stderr, "Unknown option '%s'\n", argv[1]);
    Usage();
    return 1;
  }
  if (strcmp(argv[2], "-h") == 0) {
    enable_dxva2 = true;
  } else if (strcmp(argv[2], "-s") == 0) {
    enable_dxva2 = false;
  } else {
    fprintf(stderr, "Unknown option '%s'\n", argv[2]);
    Usage();
    return 1;
  }

  // TODO(imcheng): implement the option for hardware acceleration
  fprintf(stderr, "use_renderer_sink: %s\n",
          use_renderer_sink ? "TRUE" : "FALSE");
  fprintf(stderr, "enable_dxva2: %s\n", enable_dxva2 ? "TRUE" : "FALSE");

  HRESULT hr;
  // Startup is moved from object creation to main method.
  hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(hr)) {
    fprintf(stderr, "MFStartup failed: %#X", hr);
    return 1;
  }

  // Note: This function allocates space for g_input_file_name on the heap, so
  // remember to free it at the end.
  if (ConvertANSIStringToUnicode(argv[3], &g_input_file_name) != 0) {
    fprintf(stderr, "Fatal error while converting g_input_file_name\n");
    return 1;
  }
  _fwprintf_p(stderr, L"Input file: %s\n", g_input_file_name);
  int retval;
  if (use_renderer_sink)
    retval = RunRendererApp();
  else
    retval = RunTranscoderApp(argv[4]);

  // Clean up.
  if (g_main_player) {
    g_main_player->Shutdown();
    g_main_player->Release();
  }
  MFShutdown();
  delete[] g_input_file_name;

  printf("Terminated\n");
  return retval;
}

}  // namespace mfplayer


int main(int argc, char** argv) {
  return mfplayer::main2(argc, argv);
}
