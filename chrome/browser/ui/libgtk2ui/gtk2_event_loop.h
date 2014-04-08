// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTK2UI_GTK2_EVENT_LOOP_H_
#define CHROME_BROWSER_UI_LIBGTK2UI_GTK2_EVENT_LOOP_H_

#include "base/macros.h"
#include "ui/base/glib/glib_integers.h"

typedef union _GdkEvent GdkEvent;
typedef struct _GdkEventKey GdkEventKey;

template<typename Type> struct DefaultSingletonTraits;

namespace libgtk2ui {

class Gtk2EventLoop {
 public:
  static Gtk2EventLoop* GetInstance();

 private:
  friend struct DefaultSingletonTraits<Gtk2EventLoop>;

  Gtk2EventLoop();
  ~Gtk2EventLoop();

  static void GdkEventTrampoline(GdkEvent* event, gpointer data);

  void DispatchGdkEvent(GdkEvent* gdk_event);
  void ProcessGdkEventKey(const GdkEventKey& gdk_event_key);

  DISALLOW_COPY_AND_ASSIGN(Gtk2EventLoop);
};

}  // namespace libgtk2ui

#endif  // CHROME_BROWSER_UI_LIBGTK2UI_GTK2_EVENT_LOOP_H_
