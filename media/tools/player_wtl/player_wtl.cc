// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stand alone media player application used for testing the media library.

#include "media/tools/player_wtl/player_wtl.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "media/base/pipeline_impl.h"
#include "media/filters/audio_renderer_impl.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/file_data_source.h"
#include "media/tools/player_wtl/mainfrm.h"

// See player_wtl.h to enable timing code by turning on TESTING macro.

namespace switches {
const wchar_t* const kExit = L"exit";
}  // namespace switches

CAppModule g_module;

int Run(wchar_t* win_cmd_line, int cmd_show) {
  base::AtExitManager exit_manager;

  // Windows version of Init uses OS to fetch command line.
  CommandLine::Init(0, NULL);
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  std::vector<std::wstring> filenames(cmd_line->GetLooseValues());

  CMessageLoop the_loop;
  g_module.AddMessageLoop(&the_loop);

  CMainFrame wnd_main;
  if (wnd_main.CreateEx() == NULL) {
    DCHECK(false) << "Main window creation failed!";
    return 0;
  }

  wnd_main.ShowWindow(cmd_show);

  if (!filenames.empty()) {
    const wchar_t* url = filenames[0].c_str();
    wnd_main.MovieOpenFile(url);
  }

  if (cmd_line->HasSwitch(switches::kExit)) {
    wnd_main.OnOptionsExit(0, 0, 0);
  }

  int result = the_loop.Run();

  media::Movie::get()->Close();

  g_module.RemoveMessageLoop();
  return result;
}

int WINAPI _tWinMain(HINSTANCE instance, HINSTANCE /*previous_instance*/,
                     wchar_t* cmd_line, int cmd_show) {
#ifdef TESTING
  double player_time_start = GetTime();
#endif
  INITCOMMONCONTROLSEX iccx;
  iccx.dwSize = sizeof(iccx);
  iccx.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES;
  if (!::InitCommonControlsEx(&iccx)) {
    DCHECK(false) << "Failed to initialize common controls";
    return 1;
  }
  if (FAILED(g_module.Init(NULL, instance))) {
    DCHECK(false) << "Failed to initialize application module";
    return 1;
  }
  int result = Run(cmd_line, cmd_show);

  g_module.Term();
#ifdef TESTING
  double player_time_end = GetTime();
  char outputbuf[512];
  _snprintf_s(outputbuf, sizeof(outputbuf),
              "player time %5.2f ms\n",
              player_time_end - player_time_start);
  OutputDebugStringA(outputbuf);
  printf("%s", outputbuf);
#endif
  return result;
}
