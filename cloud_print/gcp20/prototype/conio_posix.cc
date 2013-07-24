// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/conio_posix.h"

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

void SetTemporaryTermiosSettings(bool temporary) {
  static termios oldt, newt;

  if (temporary) {
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ICANON;  // Disable buffered IO.
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  } else {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);  // Restore default settings.
  }
}

int _kbhit() {
  SetTemporaryTermiosSettings(true);

  timeval tv;
  fd_set rdfs;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&rdfs);
  FD_SET(STDIN_FILENO, &rdfs);
  select(STDIN_FILENO + 1, &rdfs, NULL, NULL, &tv);
  SetTemporaryTermiosSettings(false);

  return FD_ISSET(STDIN_FILENO, &rdfs);
}

int _getche() {
  SetTemporaryTermiosSettings(true);
  int c = getchar();
  SetTemporaryTermiosSettings(false);
  return c;
}

