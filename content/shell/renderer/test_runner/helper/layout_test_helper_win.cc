// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static BOOL font_smoothing_enabled = FALSE;

static void SaveInitialSettings() {
  ::SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &font_smoothing_enabled, 0);
}

// Technically, all we need to do is disable ClearType. However,
// for some reason, the call to SPI_SETFONTSMOOTHINGTYPE doesn't
// seem to work, so we just disable font smoothing all together
// (which works reliably).
static void InstallLayoutTestSettings() {
  ::SystemParametersInfo(SPI_SETFONTSMOOTHING, FALSE, 0, 0);
}

static void RestoreInitialSettings() {
  ::SystemParametersInfo(
      SPI_SETFONTSMOOTHING, static_cast<UINT>(font_smoothing_enabled), 0, 0);
}

static void SimpleSignalHandler(int signalNumber) {
  // Try to restore the settings and then go down cleanly.
  RestoreInitialSettings();
  exit(128 + signalNumber);
}

int main(int, char**) {
  // Hooks the ways we might get told to clean up...
  signal(SIGINT, SimpleSignalHandler);
  signal(SIGTERM, SimpleSignalHandler);

  SaveInitialSettings();

  InstallLayoutTestSettings();

  // Let the script know we're ready.
  printf("ready\n");
  fflush(stdout);

  // Wait for any key (or signal).
  getchar();

  RestoreInitialSettings();

  return EXIT_SUCCESS;
}
