// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/basictypes.h"
#include "mojo/public/system/core.h"
#include "mojo/system/core_impl.h"

#if defined(OS_WIN)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define SAMPLE_APP_EXPORT __declspec(dllexport)
#else
#define CDECL
#define SAMPLE_APP_EXPORT __attribute__((visibility("default")))
#endif

char* ReadStringFromPipe(mojo::Handle pipe) {
  uint32_t len = 0;
  char* buf = NULL;
  MojoResult result = mojo::ReadMessage(pipe, buf, &len, NULL, NULL,
                                        MOJO_READ_MESSAGE_FLAG_NONE);
  if (result == MOJO_RESULT_RESOURCE_EXHAUSTED) {
    buf = new char[len];
    result = mojo::ReadMessage(pipe, buf, &len, NULL, NULL,
                               MOJO_READ_MESSAGE_FLAG_NONE);
  }
  if (result < MOJO_RESULT_OK) {
    // Failure..
    if (buf)
      delete[] buf;
    return NULL;
  }
  return buf;
}

class SampleMessageWaiter {
 public:
  explicit SampleMessageWaiter(mojo::Handle pipe) : pipe_(pipe) {}
  ~SampleMessageWaiter() {}

  void Read() {
    char* string = ReadStringFromPipe(pipe_);
    if (string) {
      printf("Read string from pipe: %s\n", string);
      delete[] string;
      string = NULL;
    }
  }

  void WaitAndRead() {
    MojoResult result = mojo::Wait(pipe_, MOJO_WAIT_FLAG_READABLE, 100);
    if (result < MOJO_RESULT_OK) {
      // Failure...
    }

    Read();
  }

 private:

  mojo::Handle pipe_;
  DISALLOW_COPY_AND_ASSIGN(SampleMessageWaiter);
};

extern "C" SAMPLE_APP_EXPORT MojoResult CDECL MojoMain(
    mojo::Handle pipe) {
  SampleMessageWaiter(pipe).WaitAndRead();
  return MOJO_RESULT_OK;
}
