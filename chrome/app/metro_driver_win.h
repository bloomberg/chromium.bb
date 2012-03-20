// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_METRO_DRIVER_WIN_H_
#define CHROME_APP_METRO_DRIVER_WIN_H_

#include <Windows.h>

// Helper class to manage the metro driver dll. When present in the system,
// the main process thread needs to call InitMetro(), normal execution of
// chrome initialization will continue on a second thread while the main
// thread will be servicing the metro message loop.
class MetroDriver {
 public:
  typedef int (*MainFn)(HINSTANCE instance);

  MetroDriver();
  // returns true if chrome is being launched in metro. If so we should
  // call RunInMetro(). If not then we should just run chrome as usual.
  bool in_metro_mode() const {  return (NULL != init_metro_fn_); }

  // Enter the metro main function, which will only return when chrome metro
  // is closed. Once metro has initialized, the dll creates a new thread
  // which runs |main_fn|. This method returns when the chrome metro session
  // is closed by the user.
  int RunInMetro(HINSTANCE instance, MainFn main_fn);

 private:
  void* init_metro_fn_;
};

#endif  // CHROME_APP_METRO_DRIVER_WIN_H_
